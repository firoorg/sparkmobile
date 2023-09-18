#include "../include/spark.h"
#include "../src/spark.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

namespace spark {

using namespace secp_primitives;

// Generate a random char vector from a random scalar
static std::vector<unsigned char> random_char_vector() {
    Scalar temp;
    temp.randomize();
    std::vector<unsigned char> result;
    result.resize(SCALAR_ENCODING);
    temp.serialize(result.data());

    return result;
}

class SparkTest {};

BOOST_FIXTURE_TEST_SUITE(full_tests, SparkTest)

BOOST_AUTO_TEST_CASE(generate_verify)
{
    auto* params = spark::Params::get_default();
    // Generate all spark keys, use HD key genetation to generate spend key,
    Scalar r_;
    r_.randomize();
    SpendKey spend_key(params, r_);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);

    Address address(incoming_view_key, uint64_t(1));

    uint64_t v = 100;
    std::vector<spark::MintedCoinData> outputs{{address, uint64_t(100), "memo"}, {address, uint64_t(200), "memo"}};

    std::vector<unsigned char> serial_context = {};
    std::vector<CRecipient> recipients = createSparkMintRecipients(outputs, serial_context, true);

    std::vector<CScript> scripts;
    scripts.push_back(recipients[0].pubKey);
    scripts.push_back(recipients[1].pubKey);

    MintTransaction mintTransaction(params);
    BOOST_CHECK_NO_THROW(ParseSparkMintTransaction(scripts, mintTransaction));
    BOOST_TEST(mintTransaction.verify());

    std::vector<Coin> coins; // this is our anonymity set, when using this api, you need to get it from outside, ex. from electrumX server
    mintTransaction.getCoins(coins);
    BOOST_TEST(coins.size() == 2);

    std::list<CSparkMintMeta> in_coins;
    in_coins.push_back(getMetadata(coins[0], incoming_view_key));
    in_coins.push_back(getMetadata(coins[1], incoming_view_key));

    CAmount spendAmount = 110;
    std::vector< CSparkMintMeta> coinsToSpend;
    int64_t changeToMint;
    BOOST_TEST(GetCoinsToSpend(spendAmount, coinsToSpend, in_coins, changeToMint));

    std::vector<spark::InputCoinData> inputs;
    spark::InputCoinData inputCoinData = getInputData(coinsToSpend[0].coin, full_view_key, incoming_view_key);
    inputCoinData.cover_set_id = 0;
    inputCoinData.index = 1;
    inputs.push_back(inputCoinData);

    std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data;
    spark::CoverSetData set_data;
    set_data.cover_set = coins;
    set_data.cover_set_representation = random_char_vector();
    cover_set_data[0] = set_data;
    std::map<uint64_t, uint256> idAndBlockHashes;
    idAndBlockHashes[0] = uint256();

    std::vector<spark::OutputCoinData> privOutputs{{address, uint64_t(100), "memo"}};
    std::vector<uint8_t> inputScript;
    std::vector<std::vector<unsigned char>> outputScripts;
    BOOST_CHECK_NO_THROW(getSparkSpendScripts(full_view_key, spend_key, inputs, cover_set_data, idAndBlockHashes, uint64_t(1), uint64_t(99), privOutputs, inputScript, outputScripts));
}

BOOST_AUTO_TEST_SUITE_END()

}
