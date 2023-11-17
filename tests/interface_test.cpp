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
 * Test the correctness of the getAddress function.
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

    // Print a newline then a message that getAddress debugging messages will follow.
    std::cout << std::endl;
    std::cout << "getAddress debugging messages:" << std::endl;

    // Output the addresses for debugging.
    std::cout << "Address from Interface: " << addressFromInterface << std::endl;
    std::cout << "Directly constructed address: " << encodedDirectAddress << std::endl;

    // Print a newline to the console.
    std::cout << std::endl;
}

/*
 * Debug function to develop a CCoin->Coin convertToCppStruct.
 */
BOOST_AUTO_TEST_CASE(convertToCppStruct_test) {
    const char* keyDataHex = "0000000000000000000000000000000000000000000000000000000000000000"; // Example key data in hex.
    int index = 1;
    // int diversifier = 0;

    // Derive a SpendKey spendKey from the keyDataHex and index.
    // SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);

    // Get the SpendKey Scalar k.
    // const Params* params;
    // params = Params::get_default();
    // FullViewKey fullViewKey(spendKey);
    // IncomingViewKey incomingViewKey(fullViewKey);
    // Address address(incomingViewKey, static_cast<uint64_t>(diversifier));

    // Generate a random nonce k:
    Scalar k;
    k.randomize();
    // Serialize Scalar k to byte array.
    std::vector<unsigned char> kBytes(32); // Scalar is typically 32 bytes.
    k.serialize(kBytes.data());

    // Construct a CCoin.
    std::string memo = "Foo";
    uint64_t v = 123; // arbitrary value
    std::vector<unsigned char> serial_context = {0, 1, 2, 3, 4, 5, 6, 7};
    CCoin ccoin(COIN_TYPE_MINT, kBytes.data(), kBytes.size(), keyDataHex, index, v,
    reinterpret_cast<const unsigned char*>(memo.c_str()), memo.size(),
    serial_context.data(), serial_context.size());

    // Convert the coin to a C++ Coin struct.
    //
    // Use convertToCppStruct from utils.
    Coin coin = convertToCppStruct(ccoin);

    // Compare the coins.
    BOOST_CHECK_EQUAL(coin.type, ccoin.type);
    BOOST_CHECK_EQUAL(coin.v, ccoin.v);
    // BOOST_CHECK_EQUAL(coin.serial_context, ccoin.serial_context);
    // Can't check many more fields because they're private.

    // Print a message that convertToCppStruct debugging messages will follow.
    std::cout << "convertToCppStruct debugging messages:" << std::endl;

    // Print some information comparing the CCoin and Coin.
    std::cout << "CCoin v: " << ccoin.v << std::endl;
    std::cout << "Coin v: " << coin.v << std::endl;
    std::cout << "CCoin serial_context: " << bin2hex(ccoin.serial_context, ccoin.serial_contextLength) << std::endl;
    std::cout << "Coin serial_context: " << bin2hex(coin.serial_context, coin.serial_context.size()) << std::endl;

    // Print a newline to the console.
    std::cout << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()

}
