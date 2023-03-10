#include "../include/spark.h"
//#include "../bitcoin/amount.h"
//#include <iostream>
//
#define SPARK_INPUT_LIMIT_PER_TRANSACTION            50

#define SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION     (10000 * COIN)

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

spark::Coin getCoinFromMeta(const CSparkMintMeta& meta, const std::vector<unsigned char> serialized_viewkey)
{
    const auto* params = spark::Params::get_default();
    const spark::IncomingViewKey viewKey(params, serialized_viewkey);
    spark::Address address(viewKey, meta.i);

    return spark::Coin(params, meta.type, meta.k, address, meta.v, meta.memo, meta.serial_context);
}