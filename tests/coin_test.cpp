#include "../src/coin.h"
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

class SparkTest
{
};

BOOST_FIXTURE_TEST_SUITE(spark_coin_tests, SparkTest)

BOOST_AUTO_TEST_CASE(mint_identify_recover)
{
    // Parameters
    const Params* params;
    params = Params::get_default();
    
    const uint64_t i = 12345;
    const uint64_t v = 86;
    const std::string memo = "Spam and eggs are a tasty dish!"; // maximum length
    BOOST_CHECK_EQUAL(memo.size(), params->get_memo_bytes());

    // Generate keys
    SpendKey spend_key(params);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);

    // Generate address
    Address address(incoming_view_key, i);

    // Generate coin
    Scalar k;
    k.randomize();
    Coin coin = Coin(
        params,
        COIN_TYPE_MINT,
        k,
        address,
        v,
        memo,
        random_char_vector()
    );

    // Identify coin
    IdentifiedCoinData i_data = coin.identify(incoming_view_key);
    BOOST_CHECK_EQUAL(i_data.i, i);
    BOOST_CHECK_EQUAL_COLLECTIONS(i_data.d.begin(), i_data.d.end(), address.get_d().begin(), address.get_d().end());
    BOOST_CHECK_EQUAL(i_data.v, v);
    BOOST_CHECK_EQUAL(i_data.k, k);
    BOOST_CHECK_EQUAL(i_data.memo, memo);

    // Recover coin
    RecoveredCoinData r_data = coin.recover(full_view_key, i_data);
    BOOST_CHECK_EQUAL(
        params->get_F()*(SparkUtils::hash_ser(k, coin.serial_context) + SparkUtils::hash_Q2(incoming_view_key.get_s1(), i) + full_view_key.get_s2()) + full_view_key.get_D(),
        params->get_F()*r_data.s + full_view_key.get_D()
    );
    BOOST_CHECK_EQUAL(r_data.T*r_data.s + full_view_key.get_D(), params->get_U());
}

BOOST_AUTO_TEST_CASE(spend_identify_recover)
{
    // Parameters
    const Params* params;
    params = Params::get_default();
    
    const uint64_t i = 12345;
    const uint64_t v = 86;
    const std::string memo = "Spam and eggs";

    // Generate keys
    SpendKey spend_key(params);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);

    // Generate address
    Address address(incoming_view_key, i);

    // Generate coin
    Scalar k;
    k.randomize();
    Coin coin = Coin(
        params,
        COIN_TYPE_SPEND,
        k,
        address,
        v,
        memo,
        random_char_vector()
    );

    // Identify coin
    IdentifiedCoinData i_data = coin.identify(incoming_view_key);
    BOOST_CHECK_EQUAL(i_data.i, i);
    BOOST_CHECK_EQUAL_COLLECTIONS(i_data.d.begin(), i_data.d.end(), address.get_d().begin(), address.get_d().end());
    BOOST_CHECK_EQUAL(i_data.v, v);
    BOOST_CHECK_EQUAL(i_data.k, k);
    BOOST_CHECK_EQUAL(i_data.memo, memo);
    // Recover coin
    RecoveredCoinData r_data = coin.recover(full_view_key, i_data);
    BOOST_CHECK_EQUAL(
        params->get_F()*(SparkUtils::hash_ser(k, coin.serial_context) + SparkUtils::hash_Q2(incoming_view_key.get_s1(), i) + full_view_key.get_s2()) + full_view_key.get_D(),
        params->get_F()*r_data.s + full_view_key.get_D()
    );
    BOOST_CHECK_EQUAL(r_data.T*r_data.s + full_view_key.get_D(), params->get_U());
}

BOOST_AUTO_TEST_CASE(rejects_short_recipient_memo_payloads)
{
    const Params* params = Params::get_default();
    SpendKey spend_key(params);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);
    Address address(incoming_view_key, 12345);
    Scalar k;
    k.randomize();
    const std::vector<unsigned char> serial_context = random_char_vector();

    const auto replace_recipient_data = [&address, &k](
            Coin& coin,
            auto recipient_data,
            const std::string& label) {
        recipient_data.padded_memo.assign(1, '\0');
        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << recipient_data;
        stream.resize(coin.r_.ciphertext.size());
        coin.r_ = AEAD::encrypt(
            address.get_Q1()*SparkUtils::hash_k(k), label, stream);
    };

    Coin mint(
        params, COIN_TYPE_MINT, k, address, 86, "", serial_context);
    MintCoinRecipientData mint_data;
    mint_data.d = address.get_d();
    mint_data.k = k;
    replace_recipient_data(mint, mint_data, "Mint coin data");
    BOOST_CHECK_THROW(mint.identify(incoming_view_key), std::runtime_error);

    Coin spend(
        params, COIN_TYPE_SPEND, k, address, 86, "", serial_context);
    SpendCoinRecipientData spend_data;
    spend_data.v = 86;
    spend_data.d = address.get_d();
    spend_data.k = k;
    replace_recipient_data(spend, spend_data, "Spend coin data");
    BOOST_CHECK_THROW(spend.identify(incoming_view_key), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rejects_invalid_recipient_keys)
{
    const Params* params = Params::get_default();
    Address invalid_address(params);
    Scalar k;
    k.randomize();

    BOOST_CHECK_THROW(
        Coin(params, COIN_TYPE_MINT, k, invalid_address, 86, "", {}),
        std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()

}
