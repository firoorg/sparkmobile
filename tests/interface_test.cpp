/*
 * Interface "tests"
 *
 * Some tests are just to debug functions while they're developed.
 *
 * Compile with `./interface bin` and run with  `./bin/interface_test`, as in:
 *
 * ```
 * ./interface bin && ./bin/interface_test
 * ```
 */

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

 /*
  * Test the correctness of the getAddressFromData function.
  */
 BOOST_AUTO_TEST_CASE(getAddress_test) {
     const char* keyDataHex = "0000000000000000000000000000000000000000000000000000000000000000"; // Example key data in hex.
     int index = 1;
     int diversifier = 0;
     int isTestNet = 1; // Yes.

     // Use the getAddress from Dart interface.
     const char* addressFromInterface = getAddress(keyDataHex, index, diversifier, isTestNet);

     // Directly construct the address using Spark library.
     SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);
     FullViewKey fullViewKey(spendKey);
     IncomingViewKey incomingViewKey(fullViewKey);
     Address directAddress(incomingViewKey, static_cast<uint64_t>(diversifier));

     std::string encodedDirectAddress = directAddress.encode(isTestNet ? ADDRESS_NETWORK_TESTNET : ADDRESS_NETWORK_MAINNET);

     // Compare the two addresses.
     BOOST_CHECK_EQUAL(std::string(addressFromInterface), encodedDirectAddress);

     // Output the addresses for debugging.
     std::cout << "Address from Interface: " << addressFromInterface << std::endl;
     std::cout << "Directly constructed address: " << encodedDirectAddress << std::endl;
 }

BOOST_AUTO_TEST_SUITE_END()

}
