#include "../include/spark.h"
#include "spark.h"
//#include "../bitcoin/amount.h"
//#include <iostream>

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
        balance += start->v;
    }
    return balance;
}

std::vector<CRecipient> createSparkMintRecipients(
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
        std::vector<CSparkMintMeta>& coinsToSpend_out,
        std::list<CSparkMintMeta> coins,
        int64_t& changeToMint)
{
    CAmount availableBalance = CalculateSparkCoinsBalance(coins.begin(), coins.end());

    if (required > availableBalance) {
        throw std::runtime_error("Insufficient funds !");
    }

    // sort by biggest amount. if it is same amount we will prefer the older block
    auto comparer = [](const CSparkMintMeta& a, const CSparkMintMeta& b) -> bool {
        return a.v != b.v ? a.v > b.v : a.nHeight < b.nHeight;
    };
    coins.sort(comparer);

    CAmount spend_val(0);

    std::list<CSparkMintMeta> coinsToSpend;
    while (spend_val < required) {
        if (coins.empty())
            break;

        CSparkMintMeta choosen;
        CAmount need = required - spend_val;

        auto itr = coins.begin();
        if (need >= itr->v) {
            choosen = *itr;
            coins.erase(itr);
        } else {
            for (auto coinIt = coins.rbegin(); coinIt != coins.rend(); coinIt++) {
                auto nextItr = coinIt;
                nextItr++;

                if (coinIt->v >= need && (nextItr == coins.rend() || nextItr->v != coinIt->v)) {
                    choosen = *coinIt;
                    coins.erase(std::next(coinIt).base());
                    break;
                }
            }
        }

        spend_val += choosen.v;
        coinsToSpend.push_back(choosen);
    }

    // sort by group id ay ascending order. it is mandatory for creting proper joinsplit
    auto idComparer = [](const CSparkMintMeta& a, const CSparkMintMeta& b) -> bool {
        return a.nId < b.nId;
    };
    coinsToSpend.sort(idComparer);

    changeToMint = spend_val - required;
    coinsToSpend_out.insert(coinsToSpend_out.begin(), coinsToSpend.begin(), coinsToSpend.end());

    return true;
}

std::pair<CAmount, std::vector<CSparkMintMeta>> SelectSparkCoins(
        CAmount required,
        bool subtractFeeFromAmount,
        std::list<CSparkMintMeta> coins,
        std::size_t mintNum,
        std::size_t utxoNum,
        std::size_t additionalTxSize) {
    CFeeRate fRate;

    CAmount fee;
    unsigned size;
    int64_t changeToMint = 0; // this value can be negative, that means we need to spend remaining part of required value with another transaction (nMaxInputPerTransaction exceeded)

    std::vector<CSparkMintMeta> spendCoins;
    for (fee = fRate.GetFeePerK();;) {
        CAmount currentRequired = required;

        if (!subtractFeeFromAmount)
            currentRequired += fee;
        spendCoins.clear();
        if (!GetCoinsToSpend(currentRequired, spendCoins, coins, changeToMint)) {
            throw std::invalid_argument("Unable to select cons for spend");
        }

        // 1803 is for first grootle proof/aux data
        // 213 for each private output, 34 for each utxo,924 constant parts of tx parts of tx,
        size = 924 + 1803*(spendCoins.size()) + 322*(mintNum+1) + 34 * utxoNum + additionalTxSize;
        CAmount feeNeeded = size;

        if (fee >= feeNeeded) {
            break;
        }

        fee = feeNeeded;

        if (subtractFeeFromAmount)
            break;
    }

    if (changeToMint < 0)
        throw std::invalid_argument("Unable to select cons for spend");

    return std::make_pair(fee, spendCoins);

}


bool getIndex(const spark::Coin& coin, const std::vector<spark::Coin>& anonymity_set, size_t& index) {
    for (std::size_t j = 0; j < anonymity_set.size(); ++j) {
        if (anonymity_set[j] == coin){
            index = j;
            return true;
        }
    }
    return false;
}

void createSparkSpendTransaction(
        const spark::SpendKey& spendKey,
        const spark::FullViewKey& fullViewKey,
        const spark::IncomingViewKey& incomingViewKey,
        const std::vector<std::pair<CAmount, bool>>& recipients,
        const std::vector<std::pair<spark::OutputCoinData, bool>>& privateRecipients,
        std::list<CSparkMintMeta> coins,
        const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data_all,
        const std::map<uint64_t, uint256>& idAndBlockHashes_all,
        const uint256& txHashSig,
        std::size_t additionalTxSize,
        CAmount &fee,
        std::vector<uint8_t>& serializedSpend,
        std::vector<std::vector<unsigned char>>& outputScripts,
        std::vector<CSparkMintMeta>& spentCoinsOut) {

    if (recipients.empty() && privateRecipients.empty()) {
        throw std::runtime_error("Either recipients or newMints has to be nonempty.");
    }

    if (privateRecipients.size() >= SPARK_OUT_LIMIT_PER_TX - 1)
        throw std::runtime_error("Spark shielded output limit exceeded.");

    // calculate total value to spend
    CAmount vOut = 0;
    CAmount mintVOut = 0;
    unsigned recipientsToSubtractFee = 0;

    for (size_t i = 0; i < recipients.size(); i++) {
        auto& recipient = recipients[i];

        if (!MoneyRange(recipient.first)) {
            throw std::runtime_error("Recipient has invalid amount");
        }

        vOut += recipient.first;

        if (recipient.second) {
            recipientsToSubtractFee++;
        }
    }

    for (const auto& privRecipient : privateRecipients) {
        mintVOut += privRecipient.first.v;
        if (privRecipient.second) {
            recipientsToSubtractFee++;
        }
    }

    if (vOut > SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION)
        throw std::runtime_error("Spend to transparent address limit exceeded (10,000 Firo per transaction).");

    std::pair<CAmount, std::vector<CSparkMintMeta>> estimated =
            SelectSparkCoins(vOut + mintVOut, recipientsToSubtractFee, coins, privateRecipients.size(), recipients.size(), additionalTxSize);

    auto recipients_ = recipients;
    auto privateRecipients_ = privateRecipients;
    {
        bool remainderSubtracted = false;
        fee = estimated.first;
        for (size_t i = 0; i < recipients_.size(); i++) {
            auto &recipient = recipients_[i];

            if (recipient.second) {
                // Subtract fee equally from each selected recipient.
                recipient.first -= fee / recipientsToSubtractFee;

                if (!remainderSubtracted) {
                    // First receiver pays the remainder not divisible by output count.
                    recipient.first -= fee % recipientsToSubtractFee;
                    remainderSubtracted = true;
                }
            }
        }

        for (size_t i = 0; i < privateRecipients_.size(); i++) {
            auto &privateRecipient = privateRecipients_[i];

            if (privateRecipient.second) {
                // Subtract fee equally from each selected recipient.
                privateRecipient.first.v -= fee / recipientsToSubtractFee;

                if (!remainderSubtracted) {
                    // First receiver pays the remainder not divisible by output count.
                    privateRecipient.first.v -= fee % recipientsToSubtractFee;
                    remainderSubtracted = true;
                }
            }
        }

    }

    const spark::Params* params = spark::Params::get_default();
    if (spendKey == spark::SpendKey(params))
        throw std::runtime_error("Invalid pend key.");

    CAmount spendInCurrentTx = 0;
    for (auto& spendCoin : estimated.second)
        spendInCurrentTx += spendCoin.v;
    spendInCurrentTx -= fee;

    uint64_t transparentOut = 0;
    // fill outputs
    for (size_t i = 0; i < recipients_.size(); i++) {
        auto& recipient = recipients_[i];
        if (recipient.first == 0)
            continue;

        transparentOut += recipient.first;
    }

    spendInCurrentTx -= transparentOut;
    std::vector<spark::OutputCoinData> privOutputs;
    // fill outputs
    for (size_t i = 0; i < privateRecipients_.size(); i++) {
        auto& recipient = privateRecipients_[i];
        if (recipient.first.v == 0)
            continue;

        CAmount recipientAmount = recipient.first.v;
        spendInCurrentTx -= recipientAmount;
        spark::OutputCoinData output = recipient.first;
        output.v = recipientAmount;
        privOutputs.push_back(output);
    }

    if (spendInCurrentTx < 0)
        throw std::invalid_argument("Unable to create spend transaction.");

    if (!privOutputs.size() || spendInCurrentTx > 0) {
        spark::OutputCoinData output;
        output.address = spark::Address(incomingViewKey, SPARK_CHANGE_D);
        output.memo = "";
        if (spendInCurrentTx > 0)
            output.v = spendInCurrentTx;
        else
            output.v = 0;
        privOutputs.push_back(output);
    }

    // clear vExtraPayload to calculate metadata hash correctly
    serializedSpend.clear();

    // We will write this into cover set representation, with anonymity set hash
    uint256 sig = txHashSig;

    std::vector<spark::InputCoinData> inputs;
    std::map<uint64_t, uint256> idAndBlockHashes;
    std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data;
    for (auto& coin : estimated.second) {
        uint64_t groupId = coin.nId;
        if (cover_set_data.count(groupId) == 0) {
            if (!(cover_set_data_all.count(groupId) > 0 && idAndBlockHashes_all.count(groupId) > 0 ))
                throw std::runtime_error("No such coin in set in input data");
            cover_set_data[groupId] = cover_set_data_all.at(groupId);
            idAndBlockHashes[groupId] = idAndBlockHashes_all.at(groupId);
            cover_set_data[groupId].cover_set_representation.insert(cover_set_data[groupId].cover_set_representation.end(), sig.begin(), sig.end());

        }

        spark::InputCoinData inputCoinData;
        inputCoinData.cover_set_id = groupId;
        std::size_t index = 0;
        if (!getIndex(coin.coin, cover_set_data[groupId].cover_set, index))
            throw std::runtime_error("No such coin in set");
        inputCoinData.index = index;
        inputCoinData.v = coin.v;
        inputCoinData.k = coin.k;

        spark::IdentifiedCoinData identifiedCoinData;
        identifiedCoinData.i = coin.i;
        identifiedCoinData.d = coin.d;
        identifiedCoinData.v = coin.v;
        identifiedCoinData.k = coin.k;
        identifiedCoinData.memo = coin.memo;
        spark::RecoveredCoinData recoveredCoinData = coin.coin.recover(fullViewKey, identifiedCoinData);

        inputCoinData.T = recoveredCoinData.T;
        inputCoinData.s = recoveredCoinData.s;
        inputs.push_back(inputCoinData);
    }

    spark::SpendTransaction spendTransaction(params, fullViewKey, spendKey, inputs, cover_set_data, fee, transparentOut, privOutputs);
    spendTransaction.setBlockHashes(idAndBlockHashes);
    CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
    serialized << spendTransaction;
    serializedSpend.assign(serialized.begin(), serialized.end());

    outputScripts.clear();
    const std::vector<spark::Coin>& outCoins = spendTransaction.getOutCoins();
    for (auto& outCoin : outCoins) {
        // construct spend script
        CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
        serialized << outCoin;
        std::vector<unsigned char> script;
        script.push_back((unsigned char)OP_SPARKSMINT);
        script.insert(script.end(), serialized.begin(), serialized.end());
        outputScripts.emplace_back(script);
    }

        spentCoinsOut = estimated.second;
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
    meta.coin = coin;

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
