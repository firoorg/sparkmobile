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
    BOOST_CHECK_NO_THROW(getSparkSpendScripts(
        full_view_key,
        spend_key,
        inputs,
        cover_set_data,
        idAndBlockHashes,
        uint64_t(1),
        uint64_t(99),
        privOutputs,
        SpendTransactionVersion::V1,
        uint256(),
        inputScript,
        outputScripts));
}

BOOST_AUTO_TEST_CASE(metadata_round_trip_and_identification_errors)
{
    const Params* params = Params::get_default();
    SpendKey spendKey(params);
    FullViewKey fullViewKey(spendKey);
    IncomingViewKey incomingViewKey(fullViewKey);
    Address address(incomingViewKey, uint64_t{42});
    Scalar nonce;
    nonce.randomize();
    const std::vector<unsigned char> serialContext{1, 2, 3};
    Coin coin(
        params,
        COIN_TYPE_SPEND,
        nonce,
        address,
        123,
        "memo",
        serialContext);

    const CSparkMintMeta meta = getMetadata(coin, incomingViewKey);
    BOOST_CHECK_EQUAL(meta.type, COIN_TYPE_SPEND);
    BOOST_CHECK(meta.serial_context == serialContext);

    const Coin rebuilt = getCoinFromMeta(meta, incomingViewKey);
    BOOST_CHECK(rebuilt.S == coin.S);
    BOOST_CHECK(rebuilt.K == coin.K);
    BOOST_CHECK(rebuilt.C == coin.C);

    SpendKey otherSpendKey(params);
    FullViewKey otherFullViewKey(otherSpendKey);
    IncomingViewKey otherIncomingViewKey(otherFullViewKey);
    BOOST_CHECK_THROW(
        getMetadata(coin, otherIncomingViewKey),
        std::runtime_error);
    BOOST_CHECK_THROW(
        getInputData(coin, otherFullViewKey, otherIncomingViewKey),
        std::runtime_error);
}

BOOST_AUTO_TEST_CASE(spark_v2_builder)
{
    auto* params = spark::Params::get_default();
    Scalar r;
    r.randomize();
    SpendKey spend_key(params, r);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);
    Address address(incoming_view_key, uint64_t(1));

    std::vector<MintedCoinData> minted{
        {address, uint64_t(6000 * COIN), "memo"},
        {address, uint64_t(6000 * COIN), "memo"}};
    std::vector<CRecipient> mintRecipients =
        createSparkMintRecipients(minted, {}, true);
    std::vector<CScript> mintScripts{
        mintRecipients[0].pubKey,
        mintRecipients[1].pubKey};
    MintTransaction mintTransaction(params);
    ParseSparkMintTransaction(mintScripts, mintTransaction);

    std::vector<Coin> coins;
    mintTransaction.getCoins(coins);
    std::list<CSparkMintMeta> inputCoins;
    for (const Coin& coin : coins) {
        CSparkMintMeta meta = getMetadata(coin, incoming_view_key);
        meta.nId = 1;
        inputCoins.push_back(meta);
    }

    // Network and height policy belongs to the caller. The library must not
    // reject an otherwise valid multi-input amount using its former cap.
    const CAmount spendAmount = 11000 * COIN;
    BOOST_REQUIRE(spendAmount > SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION);
    const auto v1Estimate = SelectSparkCoins(
        spendAmount, true, inputCoins, 0, 1, 0, SpendTransactionVersion::V1);
    const auto v2Estimate = SelectSparkCoins(
        spendAmount, true, inputCoins, 0, 1, 0, SpendTransactionVersion::V2);
    BOOST_CHECK_EQUAL(v1Estimate.second.size(), 2);
    BOOST_CHECK_EQUAL(v2Estimate.second.size(), 2);
    BOOST_CHECK_EQUAL(v2Estimate.first - v1Estimate.first, 32 + 98);

    std::vector<std::pair<CAmount, bool>> recipients{{spendAmount, true}};
    std::vector<std::pair<OutputCoinData, bool>> privateRecipients;
    std::unordered_map<uint64_t, CoverSetData> coverSetData;
    coverSetData[1] = {coins, std::vector<unsigned char>(uint256().size(), 0x11)};
    std::map<uint64_t, uint256> blockHashes{{1, uint256S("02")}};
    const uint256 txHash = uint256S("03");
    const uint256 extensionCommitment = uint256S("04");
    CAmount fee = 0;
    std::vector<uint8_t> serializedSpend;
    std::vector<std::vector<unsigned char>> outputScripts;
    std::vector<CSparkMintMeta> spentCoins;

    BOOST_CHECK_THROW(
        createSparkSpendTransaction(
            spend_key,
            full_view_key,
            incoming_view_key,
            recipients,
            privateRecipients,
            inputCoins,
            coverSetData,
            blockHashes,
            txHash,
            0,
            SpendTransactionVersion::V1,
            uint256(),
            fee,
            serializedSpend,
            outputScripts,
            spentCoins),
        std::invalid_argument);

    BOOST_CHECK_NO_THROW(createSparkSpendTransaction(
        spend_key,
        full_view_key,
        incoming_view_key,
        recipients,
        privateRecipients,
        inputCoins,
        coverSetData,
        blockHashes,
        txHash,
        0,
        SpendTransactionVersion::V2,
        extensionCommitment,
        fee,
        serializedSpend,
        outputScripts,
        spentCoins));

    BOOST_REQUIRE(!serializedSpend.empty());
    BOOST_CHECK_EQUAL(serializedSpend.front(), uint8_t{2});
    BOOST_CHECK_EQUAL(spentCoins.size(), 2);

    CDataStream serialized(serializedSpend, SER_NETWORK, PROTOCOL_VERSION);
    SpendTransaction spend(
        params,
        SpendTransactionVersion::V2,
        outputScripts.size());
    BOOST_CHECK_NO_THROW(serialized >> spend);
    BOOST_CHECK(serialized.empty());
    BOOST_CHECK(spend.getVersion() == SpendTransactionVersion::V2);
    BOOST_CHECK(spend.getExtensionCommitment() == extensionCommitment);

    std::vector<Coin> outCoins;
    for (const auto& script : outputScripts) {
        BOOST_REQUIRE(!script.empty());
        BOOST_CHECK_EQUAL(script.front(), OP_SPARKSMINT);
        CDataStream coinStream(
            std::vector<unsigned char>(script.begin() + 1, script.end()),
            SER_NETWORK,
            PROTOCOL_VERSION);
        Coin coin(params);
        coinStream >> coin;
        BOOST_CHECK(coinStream.empty());
        outCoins.push_back(coin);
    }
    spend.setOutCoins(outCoins);
    coverSetData[1].cover_set_representation.insert(
        coverSetData[1].cover_set_representation.end(),
        txHash.begin(),
        txHash.end());
    spend.setCoverSets(coverSetData);
    spend.setVout(spendAmount - fee);
    std::unordered_map<uint64_t, std::vector<Coin>> coverSets{{1, coins}};
    BOOST_CHECK(SpendTransaction::verify(spend, coverSets));
}

BOOST_AUTO_TEST_CASE(parameter_sets_are_independent)
{
    const Params* testParams = Params::get_test();
    const Params* defaultParams = Params::get_default();

    BOOST_CHECK(testParams != defaultParams);
    BOOST_CHECK_EQUAL(testParams->get_n_grootle(), 2);
    BOOST_CHECK_EQUAL(defaultParams->get_n_grootle(), 8);

    SpendKey assigned(defaultParams);
    const SpendKey testKey(testParams);
    assigned = testKey;
    BOOST_CHECK(assigned.get_params() == testParams);
}

BOOST_AUTO_TEST_SUITE_END()

}
