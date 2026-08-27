#include "../src/keys.h"
#include "../bitcoin/hash.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>



namespace spark {

using namespace secp_primitives;
class SparkTest {};

BOOST_FIXTURE_TEST_SUITE(spark_address_tests, SparkTest)

BOOST_AUTO_TEST_CASE(spend_key_derivation)
{
    const Params* params = Params::get_test();
    Scalar r1(uint64_t(1));
    Scalar r2(uint64_t(2));
    const SpendKey key1(params, r1);
    const SpendKey key1Again(params, r1);
    const SpendKey key2(params, r2);

    BOOST_CHECK(key1.get_s1() == key1Again.get_s1());
    BOOST_CHECK(key1.get_s2() == key1Again.get_s2());
    BOOST_CHECK(key1.get_s1() != key2.get_s1());
    BOOST_CHECK(key1.get_s2() == key2.get_s2());

    unsigned char seed[CSHA256::OUTPUT_SIZE];
    std::vector<unsigned char> serializedR(32);
    r1.serialize(serializedR.data());
    CHash256 hasher;
    const std::string prefix1 = "s1_generation";
    hasher.Write(reinterpret_cast<const unsigned char*>(prefix1.data()), prefix1.size());
    hasher.Write(serializedR.data(), serializedR.size());
    hasher.Finalize(seed);
    Scalar expectedS1;
    expectedS1.memberFromSeed(seed);
    BOOST_CHECK(key1.get_s1() == expectedS1);

    hasher.Reset();
    const std::string prefix2 = "s2_generation";
    hasher.Write(reinterpret_cast<const unsigned char*>(prefix2.data()), prefix2.size());
    hasher.Finalize(seed);
    Scalar expectedS2;
    expectedS2.memberFromSeed(seed);
    BOOST_CHECK(key1.get_s2() == expectedS2);
}

// Check that correct encoding and decoding succeed
BOOST_AUTO_TEST_CASE(correctness)
{
    // Parameters
    const Params* params;
    params = Params::get_test();

    // Generate keys
    SpendKey spend_key(params);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);

    // Generate address
    const uint64_t i = 12345;
    Address address(incoming_view_key, i);

    // Encode address
    std::string encoded = address.encode(ADDRESS_NETWORK_TESTNET);

    // Decode address
    Address decoded;
    decoded.decode(encoded);

    // Check correctness
    BOOST_CHECK_EQUAL_COLLECTIONS(address.get_d().begin(), address.get_d().end(), decoded.get_d().begin(), decoded.get_d().end());
    BOOST_CHECK_EQUAL(address.get_Q1(), decoded.get_Q1());
    BOOST_CHECK_EQUAL(address.get_Q2(), decoded.get_Q2());
}

// Check that a bad checksum fails
BOOST_AUTO_TEST_CASE(evil_checksum)
{
    // Parameters
    const Params* params;
    params = Params::get_test();

    // Generate keys
    SpendKey spend_key(params);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);

    // Generate address
    const uint64_t i = 12345;
    Address address(incoming_view_key, i);

    // Encode address
    std::string encoded = address.encode(ADDRESS_NETWORK_TESTNET);

    // Malleate the checksum
    encoded[encoded.size() - 1] = ~encoded[encoded.size() - 1];

    // Decode address
    Address decoded;
    BOOST_CHECK_THROW(decoded.decode(encoded), std::invalid_argument);
}

// Check that a bad prefix fails
BOOST_AUTO_TEST_CASE(evil_prefix)
{
    // Parameters
    const Params* params;
    params = Params::get_test();

    // Generate keys
    SpendKey spend_key(params);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);

    // Generate address
    const uint64_t i = 12345;
    Address address(incoming_view_key, i);

    // Encode address
    std::string encoded = address.encode(ADDRESS_NETWORK_TESTNET);

    // Malleate the prefix
    encoded[0] = 'x';

    // Decode address
    Address decoded;
    BOOST_CHECK_THROW(decoded.decode(encoded), std::invalid_argument);
}

// Check that a bad network fails
BOOST_AUTO_TEST_CASE(evil_network)
{
    // Parameters
    const Params* params;
    params = Params::get_test();

    // Generate keys
    SpendKey spend_key(params);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);

    // Generate address
    const uint64_t i = 12345;
    Address address(incoming_view_key, i);

    // Encode address
    std::string encoded = address.encode(ADDRESS_NETWORK_TESTNET);

    // Malleate the network
    encoded[1] = 'x';

    // Decode address
    Address decoded;
    BOOST_CHECK_THROW(decoded.decode(encoded), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()

}
