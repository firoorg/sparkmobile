/// Use as in `$ ./interface bin && ./bin/interface_test` to run the tests.

#include "../src/keys.h"
#include "../src/dart_interface.h"
#include "../src/utils.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include <iostream> // Just for printing.

namespace spark {

using namespace secp_primitives;
class SparkTest {};

BOOST_FIXTURE_TEST_SUITE(spark_address_tests, SparkTest)

// Test the correctness of the getAddressFromData function.
BOOST_AUTO_TEST_CASE(correctness)
{
    // KeyData for test (32 bytes of 0s).
    std::vector<unsigned char> keyData(32, 0);

    // Convert std::vector<unsigned char> to const char* for the functions.
    const char* keyDataPtr = reinterpret_cast<const char*>(keyData.data());

    // Index and diversifier for the test
    const int index = 0;
    const uint64_t diversifier = 0;

    // Generate a SpendKey and other keys using utils::createSpendKeyFromData.
    SpendKey spend_key = createSpendKeyFromData(keyDataPtr, index);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);

    // Generate an encoded address from the IncomingViewKey and a diversifier using the built-in Address constructor
    Address address(incoming_view_key, diversifier);
    std::string encoded_built_in = address.encode(ADDRESS_NETWORK_TESTNET); // Replace with actual network type if needed

    // Generate an encoded address from a keyData of all 0s, an index of 0, and a diversifier of 0 using utils::getAddressFromData
    std::string encoded_from_data = getAddressFromData(keyDataPtr, index, diversifier);

    // Compare the two encoded addresses
    BOOST_CHECK_EQUAL(encoded_built_in, encoded_from_data);

    // Additionally compare the decoded addresses:
    Address decoded_built_in;
    Address decoded_from_data;
    bool success_built_in = decoded_built_in.decode(encoded_built_in);
    bool success_from_data = decoded_from_data.decode(encoded_from_data);

    BOOST_CHECK(success_built_in && success_from_data);
    BOOST_CHECK_EQUAL_COLLECTIONS(
    decoded_built_in.get_d().begin(), decoded_built_in.get_d().end(),
    decoded_from_data.get_d().begin(), decoded_from_data.get_d().end()
    );
    BOOST_CHECK_EQUAL(decoded_built_in.get_Q1(), decoded_from_data.get_Q1());
    BOOST_CHECK_EQUAL(decoded_built_in.get_Q2(), decoded_from_data.get_Q2());

    // Print both addresses to the console.
    std::cout << "Built-in:  " << encoded_built_in << std::endl;
    std::cout << "Interface: " << encoded_from_data << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()

}
