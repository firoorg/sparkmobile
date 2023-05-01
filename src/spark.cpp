#include "../include/spark.h"
//#include "../bitcoin/amount.h"
//#include <iostream>
//
#define SPARK_INPUT_LIMIT_PER_TRANSACTION            50

#define SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION     (10000 * COIN)


spark::SpendKey createSpendKey(const SpendKeyData& data) {
    std::string nCountStr = std::to_string(data.getIndex());
    CHash256 hasher;
    std::string prefix = "r_generation";
    hasher.Write(reinterpret_cast<const unsigned char*>(prefix.c_str()), prefix.size());
    hasher.Write(data.getKeyData(), data.size());
    hasher.Write(reinterpret_cast<const unsigned char*>(nCountStr.c_str()), nCountStr.size());
    unsigned char hash[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(hash);

    secp_primitives::Scalar r;
    r.memberFromSeed(hash);
    spark::SpendKey key(spark::Params::get_default(), r);
    return key;
}

spark::FullViewKey createFullViewKey(const spark::SpendKey& spendKey) {
    return spark::FullViewKey(spendKey);
}

spark::IncomingViewKey createIncomingViewKey(const spark::FullViewKey& fullViewKey) {
    return spark::IncomingViewKey(fullViewKey);
}

template <typename Iterator>
static uint64_t CalculateSparkCoinsBalance(Iterator begin, Iterator end)
{
    uint64_t balance = 0;
    for (auto start = begin; start != end; ++start) {
        balance += start->first.v;
    }
    return balance;
}

std::vector<CRecipient> CreateSparkMintRecipients(
                        const std::vector<spark::MintedCoinData>& outputs,
                        const std::vector<unsigned char>& serial_context,
                        bool generate)
{
    const spark::Params* params = spark::Params::get_default();

    spark::MintTransaction sparkMint(params, outputs, serial_context, generate);

    if (generate && !sparkMint.verify()) {
        throw std::runtime_error("Unable to validate spark mint.");
    }

    std::vector<CDataStream> serializedCoins = sparkMint.getMintedCoinsSerialized();

    if (outputs.size() != serializedCoins.size())
        throw std::runtime_error("Spark mint output number should be equal to required number.");

    std::vector<CRecipient> results;
    results.reserve(outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        CScript script;
        script << OP_SPARKMINT;
        script.insert(script.end(), serializedCoins[i].begin(), serializedCoins[i].end());
        CRecipient recipient = {script, CAmount(outputs[i].v), false};
        results.emplace_back(recipient);
    }
    return results;
}

bool GetCoinsToSpend(
        CAmount required,
        std::vector<std::pair<spark::Coin, CSparkMintMeta>>& coinsToSpend_out,
        std::list<std::pair<spark::Coin, CSparkMintMeta>> coins,
        int64_t& changeToMint,
        const size_t coinsToSpendLimit)
{

    CAmount availableBalance = CalculateSparkCoinsBalance(coins.begin(), coins.end());

    if (required > SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION) {
        throw std::invalid_argument("The required amount exceeds spend limit");
    }

    if (required > availableBalance) {
        throw std::runtime_error("Insufficient funds !");
    }

    typedef std::pair<spark::Coin, CSparkMintMeta> CoinData;

    // sort by biggest amount. if it is same amount we will prefer the older block
    auto comparer = [](const CoinData& a, const CoinData& b) -> bool {
        return a.second.v != b.second.v ? a.second.v > b.second.v : a.second.nHeight < b.second.nHeight;
    };
    coins.sort(comparer);

    CAmount spend_val(0);

    std::list<CoinData> coinsToSpend;
    while (spend_val < required) {
        if (coins.empty())
            break;

        CoinData choosen;
        CAmount need = required - spend_val;

        auto itr = coins.begin();
        if (need >= itr->second.v) {
            choosen = *itr;
            coins.erase(itr);
        } else {
            for (auto coinIt = coins.rbegin(); coinIt != coins.rend(); coinIt++) {
                auto nextItr = coinIt;
                nextItr++;

                if (coinIt->second.v >= need && (nextItr == coins.rend() || nextItr->second.v != coinIt->second.v)) {
                    choosen = *coinIt;
                    coins.erase(std::next(coinIt).base());
                    break;
                }
            }
        }

        spend_val += choosen.second.v;
        coinsToSpend.push_back(choosen);

        if (coinsToSpend.size() == coinsToSpendLimit) // if we pass input number limit, we stop and try to spend remaining part with another transaction
            break;
    }

    // sort by group id ay ascending order. it is mandatory for creting proper joinsplit
    auto idComparer = [](const CoinData& a, const CoinData& b) -> bool {
        return a.second.nId < b.second.nId;
    };
    coinsToSpend.sort(idComparer);

    changeToMint = spend_val - required;
    coinsToSpend_out.insert(coinsToSpend_out.begin(), coinsToSpend.begin(), coinsToSpend.end());

    return true;
}

std::vector<std::pair<CAmount, std::vector<std::pair<spark::Coin, CSparkMintMeta>>>> SelectSparkCoins(
        CAmount required,
        bool subtractFeeFromAmount,
        std::list<std::pair<spark::Coin, CSparkMintMeta>> coins,
        std::size_t mintNum) {
    CFeeRate fRate;

    std::vector<std::pair<CAmount, std::vector<std::pair<spark::Coin, CSparkMintMeta>>>> result;

    while (required > 0) {
        CAmount fee;
        unsigned size;
        int64_t changeToMint = 0; // this value can be negative, that means we need to spend remaining part of required value with another transaction (nMaxInputPerTransaction exceeded)

        std::vector<std::pair<spark::Coin, CSparkMintMeta>> spendCoins;
        CFeeRate fRate;
        for (fee = fRate.GetFeePerK();;) {
            CAmount currentRequired = required;

            if (!subtractFeeFromAmount)
                currentRequired += fee;
            spendCoins.clear();

            if (!GetCoinsToSpend(currentRequired, spendCoins, coins, changeToMint,
                                 SPARK_INPUT_LIMIT_PER_TRANSACTION)) {
                throw std::invalid_argument("Unable to select cons for spend");
            }

            // 924 is constant part, mainly Schnorr and Range proofs, 2535 is for each grootle proof/aux data
            // 213 for each private output, 144 other parts of tx,
            size = 924 + 2535 * (spendCoins.size()) + 213 * mintNum + 144; //TODO (levon) take in account also utxoNum
            CAmount feeNeeded = size; //TODO(Levon) temporary, use real estimation methods here

            if (fee >= feeNeeded) {
                break;
            }

            fee = feeNeeded;

            if (subtractFeeFromAmount)
                break;
        }

        result.push_back({fee, spendCoins});
        if (changeToMint < 0)
            required = - changeToMint;
        else
            required = 0;
    }
    return result;
}

spark::Address getAddress(const spark::IncomingViewKey& incomingViewKey, const uint64_t diversifier)
{
    return spark::Address(incomingViewKey, diversifier);
}

spark::Coin getCoinFromMeta(const CSparkMintMeta& meta, const spark::IncomingViewKey& incomingViewKey)
{
    const auto* params = spark::Params::get_default();
    spark::Address address(incomingViewKey, meta.i);
    return spark::Coin(params, meta.type, meta.k, address, meta.v, meta.memo, meta.serial_context);
}

spark::IdentifiedCoinData identifyCoin(spark::Coin coin, const spark::IncomingViewKey& incoming_view_key)
{
    return coin.identify(incoming_view_key);
}

void getSparkSpendScripts(const spark::FullViewKey& fullViewKey,
                          const spark::SpendKey& spendKey,
                          const std::vector<spark::InputCoinData>& inputs,
                          const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data,
                          const std::map<uint64_t, uint256>& idAndBlockHashes,
                          CAmount fee,
                          uint64_t transparentOut,
                          const std::vector<spark::OutputCoinData>& privOutputs,
                          std::vector<uint8_t>& inputScript, std::vector<std::vector<unsigned char>>& outputScripts)
{
    inputScript.clear();
    outputScripts.clear();
    const auto* params = spark::Params::get_default();
    spark::SpendTransaction spendTransaction(params, fullViewKey, spendKey, inputs, cover_set_data, fee, transparentOut, privOutputs);
    spendTransaction.setBlockHashes(idAndBlockHashes);
    CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
    serialized << spendTransaction;

    inputScript = std::vector<uint8_t>(serialized.begin(), serialized.end());

    const std::vector<spark::Coin>& outCoins = spendTransaction.getOutCoins();
    for (auto& outCoin : outCoins) {
        // construct spend script
        CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
        serialized << outCoin;
        std::vector<unsigned char> script;
        script.insert(script.end(), serialized.begin(), serialized.end());
        outputScripts.emplace_back(script);
    }
}

void ParseSparkMintTransaction(const std::vector<CScript>& scripts, spark::MintTransaction& mintTransaction)
{
    std::vector<CDataStream> serializedCoins;
    for (const auto& script : scripts) {
        if (!script.IsSparkMint())
            throw std::invalid_argument("Script is not a Spark mint");

        std::vector<unsigned char> serialized(script.begin() + 1, script.end());
        size_t size = spark::Coin::memoryRequired() + 8; // 8 is the size of uint64_t
        if (serialized.size() < size) {
            throw std::invalid_argument("Script is not a valid Spark mint");
        }

        CDataStream stream(
                std::vector<unsigned char>(serialized.begin(), serialized.end()),
                SER_NETWORK,
                PROTOCOL_VERSION
        );

        serializedCoins.push_back(stream);
    }
    try {
        mintTransaction.setMintTransaction(serializedCoins);
    } catch (...) {
        throw std::invalid_argument("Unable to deserialize Spark mint transaction");
    }
}

void ParseSparkMintCoin(const CScript& script, spark::Coin& txCoin)
{
    if (!script.IsSparkMint() && !script.IsSparkSMint())
        throw std::invalid_argument("Script is not a Spark mint");

    if (script.size() < 213) {
        throw std::invalid_argument("Script is not a valid Spark Mint");
    }

    std::vector<unsigned char> serialized(script.begin() + 1, script.end());
    CDataStream stream(
            std::vector<unsigned char>(serialized.begin(), serialized.end()),
            SER_NETWORK,
            PROTOCOL_VERSION
    );

    try {
        stream >> txCoin;
    } catch (...) {
        throw std::invalid_argument("Unable to deserialize Spark mint");
    }
}

CSparkMintMeta getMetadata(const spark::Coin& coin, const spark::IncomingViewKey& incoming_view_key) {
    CSparkMintMeta meta;
    spark::IdentifiedCoinData identifiedCoinData;
    try {
        identifiedCoinData = identifyCoin(coin, incoming_view_key);
    } catch (...) {
        return meta;
    }

    meta.isUsed = false;
    meta.v = identifiedCoinData.v;
    meta.memo = identifiedCoinData.memo;
    meta.d = identifiedCoinData.d;
    meta.i = identifiedCoinData.i;
    meta.k = identifiedCoinData.k;
    meta.serial_context = {};

    return meta;
}

spark::InputCoinData getInputData(spark::Coin coin, const spark::FullViewKey& full_view_key, const spark::IncomingViewKey& incoming_view_key)
{
    spark::InputCoinData inputCoinData;
    spark::IdentifiedCoinData identifiedCoinData;
    try {
        identifiedCoinData = identifyCoin(coin, incoming_view_key);
    } catch (...) {
        return inputCoinData;
    }

    spark::RecoveredCoinData recoveredCoinData = coin.recover(full_view_key, identifiedCoinData);
    inputCoinData.T = recoveredCoinData.T;
    inputCoinData.s = recoveredCoinData.s;
    inputCoinData.k = identifiedCoinData.k;
    inputCoinData.v = identifiedCoinData.v;

    return inputCoinData;
}

spark::InputCoinData getInputData(std::pair<spark::Coin, CSparkMintMeta> coin, const spark::FullViewKey& full_view_key)
{
    spark::IdentifiedCoinData identifiedCoinData;
    identifiedCoinData.k = coin.second.k;
    identifiedCoinData.v = coin.second.v;
    identifiedCoinData.d = coin.second.d;
    identifiedCoinData.memo = coin.second.memo;
    identifiedCoinData.i = coin.second.i;

    spark::RecoveredCoinData recoveredCoinData = coin.first.recover(full_view_key, identifiedCoinData);
    spark::InputCoinData inputCoinData;
    inputCoinData.T = recoveredCoinData.T;
    inputCoinData.s = recoveredCoinData.s;
    inputCoinData.k = identifiedCoinData.k;
    inputCoinData.v = identifiedCoinData.v;

    return inputCoinData;
}
