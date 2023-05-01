#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include "../include/spark.h"

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

    std::vector<CRecipient>  recipients = CreateSparkMintRecipients(outputs, random_char_vector(), true);
    BOOST_CHECK_EQUAL(recipients.size(), 3);

    const uint64_t v = 1;
    std::list<std::pair<spark::Coin, CSparkMintMeta>> coins;
    spark::Coin coin(params, 0, (Scalar().randomize()), address, v, "Test memo", random_char_vector());
    CSparkMintMeta mint;
    mint.v = v;
    mint.isUsed = false;
    coins.push_back(std::make_pair(coin, mint));

    std::vector<std::pair<CAmount, std::vector<std::pair<spark::Coin, CSparkMintMeta>>>> r = SelectSparkCoins(1, true, coins, coins.size());
    BOOST_CHECK_EQUAL(r.size(), 1);
}

BOOST_AUTO_TEST_SUITE_END()

}
