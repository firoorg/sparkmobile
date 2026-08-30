#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include "../include/spark.h"
#include "../src/spark.h"

#include <limits>

namespace spark {

    static std::vector<unsigned char> random_char_vector()
    {
          Scalar temp;
          temp.randomize();
          std::vector<unsigned char> result;
          result.resize(SCALAR_ENCODING);
          temp.serialize(result.data());
          return result;
    }

class SparkTest {};

BOOST_FIXTURE_TEST_SUITE(spark_test, SparkTest)

BOOST_AUTO_TEST_CASE(rejects_missing_spend_key_data)
{
    BOOST_CHECK_THROW(SpendKeyData(nullptr), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(spend_entry_reset_clears_transaction_hash)
{
    CSparkSpendEntry entry;
    entry.hashTx = uint256S("01");
    entry.SetNull();
    BOOST_CHECK(entry.hashTx.IsNull());
}

BOOST_AUTO_TEST_CASE(rejects_invalid_mint_amounts)
{
    const Params* params = Params::get_default();
    SpendKey spend_key(params);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);
    Address address(incoming_view_key, 1);

    BOOST_CHECK_EXCEPTION(
        createSparkMintRecipients(
            {{address, static_cast<uint64_t>(MAX_MONEY) + 1, ""}},
            {},
            false),
        std::invalid_argument,
        [](const std::invalid_argument& error) {
            return std::string(error.what()) ==
                "Spark mint amount is out of range";
        });
    BOOST_CHECK_THROW(
        createSparkMintRecipients(
            {{address, static_cast<uint64_t>(MAX_MONEY), ""},
             {address, 1, ""}},
            {},
            false),
        std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(rejects_invalid_wallet_coin_amounts)
{
    std::vector<CSparkMintMeta> selected;
    int64_t change = 0;

    CSparkMintMeta invalid;
    invalid.v = std::numeric_limits<uint64_t>::max();
    BOOST_CHECK_EXCEPTION(
        GetCoinsToSpend(1, selected, {invalid}, change),
        std::invalid_argument,
        [](const std::invalid_argument& error) {
            return std::string(error.what()) ==
                "Spark coin amount is out of range";
        });

    CSparkMintMeta maximum;
    maximum.v = MAX_MONEY;
    CSparkMintMeta extra;
    extra.v = 1;
    BOOST_CHECK_THROW(
        GetCoinsToSpend(1, selected, {maximum, extra}, change),
        std::invalid_argument);
    BOOST_CHECK_THROW(
        GetCoinsToSpend(-1, selected, {}, change),
        std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(mintCoinTest)
{
    auto* params = spark::Params::get_default();
    const spark::SpendKey spend_key(params);
    const spark::FullViewKey full_view_key(spend_key);
    const spark::IncomingViewKey incoming_view_key(full_view_key);
    const uint64_t i = 12345;

    spark::Address address(incoming_view_key, i);

    std::vector<spark::MintedCoinData> outputs;

    for (int j = 0; j < 3; ++j) {
        spark::MintedCoinData output = {address, 1, "Test memo"};
        outputs.push_back(output);
    }

    std::vector<CRecipient>  recipients = createSparkMintRecipients(outputs, random_char_vector(), true);
    BOOST_CHECK_EQUAL(recipients.size(), 3);

    const uint64_t v = 1;
    std::list<CSparkMintMeta> coins;
    spark::Coin coin(params, 0, (Scalar().randomize()), address, v, "Test memo", random_char_vector());
    CSparkMintMeta mint;
    mint.v = v;
    mint.isUsed = false;
    mint.coin = coin;
    coins.push_back(mint);

    std::pair<CAmount, std::vector<CSparkMintMeta>> r = SelectSparkCoins(
        1,
        true,
        coins,
        coins.size(),
        0,
        0,
        SpendTransactionVersion::V1);
    BOOST_CHECK_EQUAL(r.second.size(), 1);

    BOOST_CHECK_EXCEPTION(
        SelectSparkCoins(
            MAX_MONEY + 1,
            true,
            coins,
            coins.size(),
            0,
            0,
            SpendTransactionVersion::V2),
        std::invalid_argument,
        [](const std::invalid_argument& error) {
            return std::string(error.what()) ==
                "Spark spend amount is out of range";
        });
    BOOST_CHECK_THROW(
        SelectSparkCoins(
            1,
            true,
            coins,
            coins.size(),
            0,
            std::numeric_limits<std::size_t>::max(),
            SpendTransactionVersion::V2),
        std::invalid_argument);

    CAmount fee;
    std::vector<uint8_t> serializedSpend;
    std::vector<std::vector<unsigned char>> outputScripts;
    std::vector<CSparkMintMeta> spentCoins;
    BOOST_CHECK_EXCEPTION(
        createSparkSpendTransaction(
            spend_key,
            full_view_key,
            incoming_view_key,
            {{MAX_MONEY, false}, {1, false}},
            {},
            coins,
            {},
            {},
            uint256(),
            0,
            SpendTransactionVersion::V2,
            uint256(),
            fee,
            serializedSpend,
            outputScripts,
            spentCoins),
        std::runtime_error,
        [](const std::runtime_error& error) {
            return std::string(error.what()) ==
                "Recipient has invalid amount";
        });
    BOOST_CHECK_THROW(
        createSparkSpendTransaction(
            spend_key,
            full_view_key,
            incoming_view_key,
            {},
            {{{address, std::numeric_limits<uint64_t>::max(), ""}, false}},
            coins,
            {},
            {},
            uint256(),
            0,
            SpendTransactionVersion::V2,
            uint256(),
            fee,
            serializedSpend,
            outputScripts,
            spentCoins),
        std::runtime_error);
    BOOST_CHECK_EXCEPTION(
        createSparkSpendTransaction(
            spend_key,
            full_view_key,
            incoming_view_key,
            {{1, true}},
            {},
            coins,
            {},
            {},
            uint256(),
            0,
            SpendTransactionVersion::V2,
            uint256(),
            fee,
            serializedSpend,
            outputScripts,
            spentCoins),
        std::runtime_error,
        [](const std::runtime_error& error) {
            return std::string(error.what()) ==
                "Recipient amount is too small after fee deduction";
        });
}

BOOST_AUTO_TEST_SUITE_END()

}
