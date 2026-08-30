#include "../src/keys.h"
#include "../bitcoin/hash.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>



namespace spark {

using namespace secp_primitives;
class SparkTest {};

static std::string encode_address(
        const Address& address,
        const GroupElement& Q1,
        const GroupElement& Q2) {
    std::vector<unsigned char> raw(address.get_d());
    std::vector<unsigned char> component(GroupElement::serialize_size);
    Q1.serialize(component.data());
    raw.insert(raw.end(), component.begin(), component.end());
    Q2.serialize(component.data());
    raw.insert(raw.end(), component.begin(), component.end());

    const unsigned char network = ADDRESS_NETWORK_TESTNET;
    std::vector<unsigned char> scrambled =
        F4Grumble(network, raw.size()).encode(raw);
    std::vector<uint8_t> converted;
    bech32::convertbits(converted, scrambled, 8, 5, true);

    std::string hrp;
    hrp.push_back(ADDRESS_ENCODING_PREFIX);
    hrp.push_back(network);
    return bech32::encode(hrp, converted, bech32::Encoding::BECH32M);
}

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

    Scalar message(uint64_t(1));
    OwnershipProof proof;
    address.prove_own(message, spend_key, incoming_view_key, proof);
    BOOST_CHECK(decoded.verify_own(message, proof));
}

BOOST_AUTO_TEST_CASE(full_view_key_deserialization_uses_default_params)
{
    const Params* params = Params::get_default();
    FullViewKey original{SpendKey(params)};
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << original;

    FullViewKey decoded;
    stream >> decoded;
    IncomingViewKey incoming(decoded);

    BOOST_CHECK(decoded.get_params() == params);
    BOOST_CHECK_NO_THROW(Address(incoming, 12345));
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

BOOST_AUTO_TEST_CASE(rejects_invalid_address_keys)
{
    const Params* params = Params::get_test();
    SpendKey spend_key(params);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);
    Address address(incoming_view_key, 12345);
    GroupElement identity;

    for (const std::string& encoded : {
            encode_address(address, identity, address.get_Q2()),
            encode_address(address, address.get_Q1(), identity),
            encode_address(address, identity, identity)}) {
        Address decoded;
        BOOST_CHECK_THROW(decoded.decode(encoded), std::invalid_argument);
    }

    IncomingViewKey invalid_view_key(params);
    BOOST_CHECK_THROW(Address(invalid_view_key, 12345), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(rejects_noncanonical_address_encoding)
{
    const Params* params = Params::get_test();
    SpendKey spend_key(params);
    Address address(IncomingViewKey(FullViewKey(spend_key)), 12345);
    bech32::DecodeResult decoded =
        bech32::decode(address.encode(ADDRESS_NETWORK_TESTNET));

    decoded.data.back() |= 1;
    std::string encoded = bech32::encode(
        decoded.hrp, decoded.data, bech32::Encoding::BECH32M);
    Address bad_padding;
    BOOST_CHECK_THROW(bad_padding.decode(encoded), std::invalid_argument);

    decoded = bech32::decode(address.encode(ADDRESS_NETWORK_TESTNET));
    std::string short_hrp(1, ADDRESS_ENCODING_PREFIX);
    encoded = bech32::encode(
        short_hrp, decoded.data, bech32::Encoding::BECH32M);
    Address bad_hrp;
    BOOST_CHECK_THROW(bad_hrp.decode(encoded), std::invalid_argument);

    decoded = bech32::decode(address.encode(ADDRESS_NETWORK_TESTNET));
    std::vector<uint8_t> scrambled;
    BOOST_REQUIRE(bech32::convertbits(scrambled, decoded.data, 5, 8, false));
    std::vector<unsigned char> raw =
        F4Grumble(ADDRESS_NETWORK_TESTNET, scrambled.size()).decode(scrambled);
    raw[AES_BLOCKSIZE + 32] = 2;
    scrambled = F4Grumble(ADDRESS_NETWORK_TESTNET, raw.size()).encode(raw);
    decoded.data.clear();
    BOOST_REQUIRE(bech32::convertbits(decoded.data, scrambled, 8, 5, true));
    encoded = bech32::encode(decoded.hrp, decoded.data, bech32::Encoding::BECH32M);
    Address noncanonical_key;
    BOOST_CHECK_THROW(noncanonical_key.decode(encoded), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()

}
