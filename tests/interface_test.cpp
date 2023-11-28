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
#include "../src/coin.h" // For COIN_TYPE_MINT etc.

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include <iostream> // Just for printing.

namespace spark {

using namespace secp_primitives;

class SparkTest {};

BOOST_FIXTURE_TEST_SUITE(spark_address_tests, SparkTest)

/*
 * Test the correctness of the getAddress wrapper.
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
    std::cout << std::endl << "getAddress debugging messages:" << std::endl;
    std::cout << "Address from Interface      : " << addressFromInterface << std::endl;
    std::cout << "Directly constructed address: " << encodedDirectAddress << std::endl;
}

/*
 * Debug function to develop a CCoin->Coin fromFFI (formerly convertToCppStruct).
 */
BOOST_AUTO_TEST_CASE(Coin_fromFFI_test) {
    // const char* keyDataHex = "0000000000000000000000000000000000000000000000000000000000000000"; // Example key data in hex.
    // int index = 1;
    // int diversifier = 0;
    // Instead of passing these values, we just pass an address.
    const char* address = "st19m57r6grs3vwmx2el5dxuv3rdf4jjjx7tvsd4a9mrj4ezlphhaaq38wmfgt24dsmzttuntcsfjkekwd4g3ktyctj6tq2cgn2mu53df8kjyj9rstuvc78030ewugqgymvk7jf5lqgek373";

    // Generate a random nonce k and serialize it to byte array.
    Scalar k;
    k.randomize();
    std::vector<unsigned char> kBytes(32); // Scalar is typically 32 bytes.
    k.serialize(kBytes.data());

    // Construct a CCoin.
    std::string memo = "Foo";
    uint64_t v = 123; // arbitrary value
    std::vector<unsigned char> serial_context = {0, 1, 2, 3, 4, 5, 6, 7};
    CCoin ccoin = createCCoin(
        COIN_TYPE_MINT,
        kBytes.data(),
        static_cast<int>(kBytes.size()),
        address,
        v,
        reinterpret_cast<const unsigned char*>(memo.c_str()),
        static_cast<int>(memo.size()),
        serial_context.data(),
        static_cast<int>(serial_context.size())
    );

    // Convert the C coin fromFFI to a C++ Coin struct.
    Coin coin = fromFFI(ccoin);

    // Compare the coins.
    BOOST_CHECK_EQUAL(coin.type, ccoin.type);
    BOOST_CHECK_EQUAL(coin.v, ccoin.v);
    // BOOST_CHECK_EQUAL(coin.serial_context, ccoin.serial_context);
    // Can't check many more fields because they're private.

    // Print some information comparing the CCoin and Coin.
    std::cout << std::endl << "CCoin->Coin fromFFI debugging messages:" << std::endl;
    std::cout << "CCoin v: " << ccoin.v << std::endl;
    std::cout << "Coin  v: " << coin.v << std::endl;
    std::cout << "CCoin serial_context: " << bytesToHex(ccoin.serial_context, ccoin.serial_contextLength) << std::endl;
    std::cout << "Coin  serial_context: " << bytesToHex(coin.serial_context, coin.serial_context.size()) << std::endl;
}

// TODO Coin->CCoin toFFI.

/*
 * Debug function to develop a IdentifiedCoinData->CIdentifiedCoinData toFFI (formerly convertToCStruct).
 */
BOOST_AUTO_TEST_CASE(CIdentifiedCoinData_toFFI_test) {
    // Make a dummy IdentifiedCoinData.
    IdentifiedCoinData identifiedCoinData;
    identifiedCoinData.i = 1;
    identifiedCoinData.d = {1, 2, 3, 4, 5, 6, 7, 8};
    identifiedCoinData.v = 123;
    identifiedCoinData.k = Scalar(123);
    identifiedCoinData.memo = "Foo";

    // Convert the IdentifiedCoinData to a CIdentifiedCoinData.
    CIdentifiedCoinData cIdentifiedCoinData = toFFI(identifiedCoinData);

    // Compare the two structs.
    BOOST_CHECK_EQUAL(cIdentifiedCoinData.i, identifiedCoinData.i);

    // Print some information comparing the IdentifiedCoinData and CIdentifiedCoinData.
    std::cout << std::endl << "convertToCStruct debugging messages:" << std::endl;
    std::cout << "IdentifiedCoinData  i: " << identifiedCoinData.i << std::endl;
    std::cout << "CIdentifiedCoinData i: " << cIdentifiedCoinData.i << std::endl;
    std::cout << "IdentifiedCoinData  d: " << bytesToHex(identifiedCoinData.d, identifiedCoinData.d.size()) << std::endl;
    std::cout << "CIdentifiedCoinData d: " << bytesToHex(cIdentifiedCoinData.d, cIdentifiedCoinData.dLength) << std::endl;
}

/*
 * Test the correctness of the identifyCoin wrapper.
 */
BOOST_AUTO_TEST_CASE(identifyCoin_test) {
    // Make a dummy CCoin.
    const char* keyDataHex = "0000000000000000000000000000000000000000000000000000000000000000"; // Example key data in hex.
    int index = 1;
    const char* address = "st19m57r6grs3vwmx2el5dxuv3rdf4jjjx7tvsd4a9mrj4ezlphhaaq38wmfgt24dsmzttuntcsfjkekwd4g3ktyctj6tq2cgn2mu53df8kjyj9rstuvc78030ewugqgymvk7jf5lqgek373";

    // Derive a SpendKey spendKey from the keyDataHex and index.
    SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);

    // Get an IncomingViewKey from the SpendKey.
    FullViewKey fullViewKey(spendKey);
    IncomingViewKey incomingViewKey(fullViewKey);

    // Construct a CCoin.
    std::string memo = "Foo";
    uint64_t v = 123; // Arbitrary value.
    std::vector<unsigned char> serial_context = {0, 1, 2, 3, 4, 5, 6, 7};

    // Make a dummy nonce (Scalar k).
    Scalar k;
    k.randomize();
    std::vector<unsigned char> scalarBytes(32); // Scalar is typically 32 bytes.
    k.serialize(scalarBytes.data());
    CCoin ccoin = createCCoin(
        COIN_TYPE_MINT,
        scalarBytes.data(),
        static_cast<int>(scalarBytes.size()),
        address,
        v,
        reinterpret_cast<const unsigned char*>(memo.c_str()),
        static_cast<int>(memo.size()),
        serial_context.data(),
        static_cast<int>(serial_context.size())
    );

    // Use the identifyCoin from Dart interface.
    CIdentifiedCoinData identifiedCoinDataFromInterface = identifyCoin(ccoin, keyDataHex, index);

    // Directly construct the IdentifiedCoinData using Spark library.
    Coin coin = fromFFI(ccoin);
    IdentifiedCoinData identifiedCoinData = coin.identify(incomingViewKey);

    // Compare the two structs.
    BOOST_CHECK_EQUAL(identifiedCoinDataFromInterface.i, identifiedCoinData.i);
    BOOST_CHECK_EQUAL(identifiedCoinDataFromInterface.v, identifiedCoinData.v);
    // BOOST_CHECK_EQUAL(identifiedCoinDataFromInterface.k, identifiedCoinData.k.serialize().data());
    // BOOST_CHECK_EQUAL(identifiedCoinDataFromInterface.memo, identifiedCoinData.memo.c_str());
    BOOST_CHECK_EQUAL(identifiedCoinDataFromInterface.memoLength, identifiedCoinData.memo.size());

    // Output the addresses for debugging.
    std::cout << std::endl << "identifyCoin debugging messages:" << std::endl;
    std::cout << "IdentifiedCoinData from Interface: " << std::endl;
    std::cout << "  i: " << identifiedCoinDataFromInterface.i << std::endl;
    std::cout << "  v: " << identifiedCoinDataFromInterface.v << std::endl;

    // Serialize the Scalar k for both identifiedCoinData.
    unsigned char serializedK[32];
    identifiedCoinData.k.serialize(serializedK);
    std::cout << "  k: " << bytesToHex(serializedK, 32) << std::endl;

    std::cout << "  memo: " << identifiedCoinDataFromInterface.memo << std::endl;
    std::cout << "  memoLength: " << identifiedCoinDataFromInterface.memoLength << std::endl;
    std::cout << "IdentifiedCoinData directly constructed: " << std::endl;
    std::cout << "  i: " << identifiedCoinData.i << std::endl;
    std::cout << "  v: " << identifiedCoinData.v << std::endl;
    std::cout << "  k: " << bytesToHex(identifiedCoinDataFromInterface.k, 32) << std::endl;
    std::cout << "  memo: " << identifiedCoinData.memo << std::endl;
    std::cout << "  memoLength: " << identifiedCoinData.memo.size() << std::endl;
}

/*
 * Debug function to develop a CCRecipient->CRecipient fromFFI function.
 *
 * A CRecipient is a struct that contains a CScript, CAmount, and a bool (subtractFee).  We accept
 * these as a CCRecipient from the Dart interface, and convert them to a CRecipient struct.
 *
 * This function just tests and compares a CRecipient generated via built-in methods to a CRecipient
 * constructed via fromFFI.  First, we'll make dummy CScript, CAmount, and bool values, and then
 * construct a CRecipient from them.  We'll construct a CCRicipient using the same values, derive a
 * CRecipient fromFFI, and compare the two structs.
 *
 * TODO fix CRecipient pubKey (extra 2 prefix, 0 appendix).
 */
BOOST_AUTO_TEST_CASE(CRecipient_fromFFI_test) {
    // Make a dummy 32-byte pubKey.
    std::vector<unsigned char> pubKeyBytes = {0, 1, 2, 3, 4, 5, 6, 7};
    unsigned char* pubKey = pubKeyBytes.data();

    // Make a dummy uint64_t amount.
    uint64_t amount = 123;

    // Make a dummy bool.
    bool subtractFee = true;

    // Construct the dummy CCRecipient.
    CCRecipient ccrecipient = createCCRecipient(pubKey, amount, subtractFee);

    // Construct the dummy CRecipient.
    CScript cscript;
    for(int i = 0; i < 32; ++i) {
      cscript << pubKeyBytes[i];
    }
    CAmount camount = CAmount(amount);
    CRecipient crecipient = createCRecipient(cscript, camount, subtractFee);

    // Convert the CCRecipient fromFFI to a C++ CRecipient struct.
    CRecipient recipient = fromFFI(ccrecipient);

    // Compare the two structs.
    BOOST_CHECK_EQUAL(recipient.subtractFeeFromAmount, crecipient.subtractFeeFromAmount);
    BOOST_CHECK_EQUAL(recipient.amount, crecipient.amount);
    // BOOST_CHECK_EQUAL(recipient.pubKey, crecipient.pubKey);


    // Print some information comparing the CCRecipient and CRecipient.
    std::cout << std::endl << "CCRecipient->CRecipient fromFFI debugging messages:" << std::endl;
    std::cout << "CCRecipient subtractFeeFromAmount: " << ccrecipient.subtractFee << std::endl;
    std::cout << "CRecipient  subtractFeeFromAmount: " << crecipient.subtractFeeFromAmount << std::endl;
    std::cout << "CCRecipient amount: " << ccrecipient.cAmount << std::endl;
    std::cout << "CRecipient  amount: " << crecipient.amount << std::endl;
    std::cout << "CCRecipient pubKey: " << bytesToHex(ccrecipient.pubKey, 32) << std::endl;

    // Serializing CScript object to a byte array.
    /*
    std::vector<unsigned char> serializedPubKey;

    if (crecipient.pubKey.size() > 0) {
        serializedPubKey = serializeCScript(crecipient.pubKey);
    }

    std::cout << "CRecipient pubKey: " << bytesToHex(serializedPubKey, serializedPubKey.size());
    */

    std::cout << "CRecipient pubKey : TODO FIX TODO FIX TODO FIX TODO FIX TODO FIX TODO FIX TODO FIX" << std::endl;
}

/*
 * Debug function to develop a CRecipient->CCRecipient toFFI function.
 *
 * A CRecipient is a struct that contains a CScript, CAmount, and a bool (subtractFee).  We accept
 * these as a CRecipient from the C++ interface, and convert them to a CCRecipient struct for the
 * Dart FFI layer.
 *
 * This function just tests and compares a CRecipient generated via built-in methods to a
 * CCRecipient constructed via toFFI.  First, we'll make dummy CScript, CAmount, and bool values,
 * and then construct a CRecipient from them.  We'll construct a CCRicipient using the same values,
 * derive a CRecipient from toFFI, and compare the two structs.
 *
 * TODO fix CRecipient and CCRecipient pubKey.
 */
BOOST_AUTO_TEST_CASE(CCRecipient_toFFI_test) {
    // Make a dummy 32-byte pubKey.
    std::vector<unsigned char> pubKeyBytes = {0, 1, 2, 3, 4, 5, 6, 7};
    unsigned char* pubKey = pubKeyBytes.data();

    // Make a dummy uint64_t amount.
    uint64_t amount = 123;

    // Make a dummy bool.
    bool subtractFee = true;

    // Construct the dummy CCRecipient.
    CCRecipient ccrecipient = createCCRecipient(pubKey, amount, subtractFee);

    // Correctly construct CRecipient.
    CScript cscript;
    for(int i = 0; i < 32; ++i) {
        cscript << pubKeyBytes[i];
    }

    CRecipient crecipient = createCRecipient(cscript, ccrecipient.cAmount, static_cast<bool>(ccrecipient.subtractFee));

    // Now convert CRecipient back to CCRecipient using toFFI.
    CCRecipient convertedCCRecipient = toFFI(crecipient);

    // Compare the two structs.
    BOOST_CHECK_EQUAL(convertedCCRecipient.subtractFee, ccrecipient.subtractFee);
    BOOST_CHECK_EQUAL(convertedCCRecipient.cAmount, ccrecipient.cAmount);
    // BOOST_CHECK_EQUAL(crecipient.pubKey, ccrecipient.pubKey);

    // Print some information comparing the CCRecipient and CRecipient.
    std::cout << std::endl << "CRecipient->CCRecipient toFFI debugging messages:" << std::endl;
    std::cout << "CRecipient subtractFeeFromAmount: " << crecipient.subtractFeeFromAmount << std::endl;
    std::cout << "CCRecipient  subtractFeeFromAmount: " << ccrecipient.subtractFee << std::endl;
    std::cout << "CRecipient amount: " << crecipient.amount << std::endl;
    std::cout << "CCRecipient  amount: " << ccrecipient.cAmount << std::endl;

    // Serialize CScript object to a byte array.
    /*
    std::vector<unsigned char> serializedPubKey;

    if (crecipient.pubKey.size() > 0) {
        serializedPubKey = serializeCScript(crecipient.pubKey);

        std::cout << "CRecipient pubKey: " << bytesToHex(serializedPubKey, serializedPubKey.size()) << std::endl;
    }

    // Serialize CCRecipient pubKey (which is already a byte array).
    std::cout << "CCRecipient pubKey: " << bytesToHex(ccrecipient.pubKey, ccrecipient.pubKeyLength) << std::endl;
    */
    std::cout << "TODO FIX CRecipient and CCRecipient pubKey TODO FIX" << std::endl;
}

/*
 * Debug function to develop a CMintedCoinData->MintedCoinData fromFFI function.
 */
BOOST_AUTO_TEST_CASE(MintedCoinData_fromFFI_test) {
    // Make a dummy CMintedCoinData.
    CMintedCoinData cmintedCoinData;
    cmintedCoinData.address = "st19m57r6grs3vwmx2el5dxuv3rdf4jjjx7tvsd4a9mrj4ezlphhaaq38wmfgt24dsmzttuntcsfjkekwd4g3ktyctj6tq2cgn2mu53df8kjyj9rstuvc78030ewugqgymvk7jf5lqgek373";
    cmintedCoinData.value = 123;
    cmintedCoinData.memo = "Foo";

    // Convert the CMintedCoinData to a MintedCoinData.
    MintedCoinData mintedCoinData = fromFFI(cmintedCoinData);

    // Compare the two structs.
    //BOOST_CHECK_EQUAL(mintedCoinData.address, cmintedCoinData.address);
    BOOST_CHECK_EQUAL(mintedCoinData.v, cmintedCoinData.value);
    BOOST_CHECK_EQUAL(mintedCoinData.memo, cmintedCoinData.memo);

    // Print some information comparing the CMintedCoinData and MintedCoinData.
    std::cout << std::endl << "CMintedCoinData->MintedCoinData fromFFI debugging messages:" << std::endl;
    std::cout << "CMintedCoinData address: " << cmintedCoinData.address << std::endl;
    std::cout << "MintedCoinData  address: " << mintedCoinData.address.encode(ADDRESS_NETWORK_TESTNET) << std::endl;
    std::cout << "CMintedCoinData value: " << cmintedCoinData.value << std::endl;
    std::cout << "MintedCoinData  value: " << mintedCoinData.v << std::endl;
    std::cout << "CMintedCoinData memo: " << cmintedCoinData.memo << std::endl;
    std::cout << "MintedCoinData  memo: " << mintedCoinData.memo << std::endl;
}

/*
 * Debug function to develop a MintedCoinData->CMintedCoinData toFFI function.
 */
BOOST_AUTO_TEST_CASE(CMintedCoinData_toFFI_test) {
    // Make a dummy MintedCoinData.
    MintedCoinData mintedCoinData;
    spark::Address address;
    address.decode("st19m57r6grs3vwmx2el5dxuv3rdf4jjjx7tvsd4a9mrj4ezlphhaaq38wmfgt24dsmzttuntcsfjkekwd4g3ktyctj6tq2cgn2mu53df8kjyj9rstuvc78030ewugqgymvk7jf5lqgek373");
    mintedCoinData.address = address;
    mintedCoinData.v = 123;
    mintedCoinData.memo = "Foo";

    // Convert the MintedCoinData to a CMintedCoinData.
    CMintedCoinData cmintedCoinData = toFFI(mintedCoinData);

    // Compare the two structs.
    //BOOST_CHECK_EQUAL(cmintedCoinData.address, mintedCoinData.address);
    BOOST_CHECK_EQUAL(cmintedCoinData.value, mintedCoinData.v);
    BOOST_CHECK_EQUAL(cmintedCoinData.memo, mintedCoinData.memo);

    // Print some information comparing the CMintedCoinData and MintedCoinData.
    std::cout << std::endl << "MintedCoinData->CMintedCoinData toFFI debugging messages:" << std::endl;
    std::cout << "MintedCoinData  address: " << mintedCoinData.address.encode(ADDRESS_NETWORK_TESTNET) << std::endl;
    std::cout << "CMintedCoinData address: " << cmintedCoinData.address << std::endl;
    std::cout << "MintedCoinData  value: " << mintedCoinData.v << std::endl;
    std::cout << "CMintedCoinData value: " << cmintedCoinData.value << std::endl;
    std::cout << "MintedCoinData  memo: " << mintedCoinData.memo << std::endl;
    std::cout << "CMintedCoinData memo: " << cmintedCoinData.memo << std::endl;
}

/*
 * Test the correctness of the createSparkMintRecipients wrapper.
 */
BOOST_AUTO_TEST_CASE(createSparkMintRecipients_test) {
    // Sample pubkey scripts.
    std::vector<unsigned char> pubKey1 = {1, 2, 3};
    std::vector<unsigned char> pubKey2 = {4, 5, 6};

    // Convert to PubKeyScript structs.
    PubKeyScript pubKeyScripts[2];
    pubKeyScripts[0].bytes = pubKey1.data();
    pubKeyScripts[0].length = pubKey1.size();
    pubKeyScripts[1].bytes = pubKey2.data();
    pubKeyScripts[1].length = pubKey2.size();

    // Sample amounts.
    uint64_t amounts[2] = {123, 456};

    // Sample memo.
    std::string memo = "Test memo";

    // Call wrapper
    CCRecipient* ccRecipients = createSparkMintRecipients(2, pubKeyScripts, amounts, memo.c_str(), 1);

    // Manually create CRecipient objects.
    CScript script1 = createCScriptFromBytes(pubKey1.data(), pubKey1.size());
    CScript script2 = createCScriptFromBytes(pubKey2.data(), pubKey2.size());
    std::vector<CRecipient> recipients = {
        createCRecipient(script1, amounts[0], true),
        createCRecipient(script2, amounts[1], true)
    };

    // Convert to CCRecipients manually.
    std::vector<CCRecipient> expected;
    for (auto& recipient : recipients) {
        expected.push_back(toFFI(recipient));
    }

    // Compare.
    for (int i = 0; i < 2; i++) {
        BOOST_CHECK_EQUAL(ccRecipients[i].cAmount, expected[i].cAmount);
        BOOST_CHECK_EQUAL(ccRecipients[i].subtractFee, expected[i].subtractFee);
        BOOST_CHECK_EQUAL(ccRecipients[i].pubKeyLength, expected[i].pubKeyLength);
    }

    // Clean up.
    delete[] ccRecipients;

    // Print some information about the recipients for debugging.
    std::cout << std::endl << "createSparkMintRecipients debugging messages:" << std::endl;
    std::cout << "CCRecipient 1 amount: " << ccRecipients[0].cAmount << std::endl;
    std::cout << "CCRecipient 2 amount: " << ccRecipients[1].cAmount << std::endl;
    std::cout << "CCRecipient 1 subtractFee: " << ccRecipients[0].subtractFee << std::endl;
    std::cout << "CCRecipient 2 subtractFee: " << ccRecipients[1].subtractFee << std::endl;
    // TODO add pubKey messages; for now just skip to printing the expected values.
    std::cout << "Expected CCRecipient 1 amount: " << expected[0].cAmount << std::endl;
    std::cout << "Expected CCRecipient 2 amount: " << expected[1].cAmount << std::endl;
    std::cout << "Expected CCRecipient 1 subtractFee: " << expected[0].subtractFee << std::endl;
    std::cout << "Expected CCRecipient 2 subtractFee: " << expected[1].subtractFee << std::endl;
}

/*
 * Debug function to develop a OutputCoinData->COutputCoinData toFFI function.
 */
BOOST_AUTO_TEST_CASE(COutputCoinData_toFFI_test) {
    // Make a dummy COutputCoinData.
    COutputCoinData coutputCoinData;
    coutputCoinData.address = "st19m57r6grs3vwmx2el5dxuv3rdf4jjjx7tvsd4a9mrj4ezlphhaaq38wmfgt24dsmzttuntcsfjkekwd4g3ktyctj6tq2cgn2mu53df8kjyj9rstuvc78030ewugqgymvk7jf5lqgek373";
    coutputCoinData.value = 123;
    coutputCoinData.memo = "Foo";

    // Convert the COutputCoinData to a OutputCoinData.
    OutputCoinData outputCoinData = fromFFI(coutputCoinData);

    // Compare the two structs.
    //BOOST_CHECK_EQUAL(outputCoinData.address, coutputCoinData.address);
    BOOST_CHECK_EQUAL(outputCoinData.v, coutputCoinData.value);
    BOOST_CHECK_EQUAL(outputCoinData.memo, coutputCoinData.memo);

    // Print some information comparing the COutputCoinData and OutputCoinData.
    std::cout << std::endl << "COutputCoinData->OutputCoinData toFFI debugging messages:" << std::endl;
    std::cout << "COutputCoinData address: " << coutputCoinData.address << std::endl;
    std::cout << "OutputCoinData  address: " << outputCoinData.address.encode(ADDRESS_NETWORK_TESTNET) << std::endl;
    std::cout << "COutputCoinData value: " << coutputCoinData.value << std::endl;
    std::cout << "OutputCoinData  value: " << outputCoinData.v << std::endl;
    std::cout << "COutputCoinData memo: " << coutputCoinData.memo << std::endl;
    std::cout << "OutputCoinData  memo: " << outputCoinData.memo << std::endl;
}

/*
 * Debug function to develop a COutputCoinData->OutputCoinData fromFFI function.
 */
BOOST_AUTO_TEST_CASE(OutputCoinData_fromFFI_test) {
    // Make a dummy COutputCoinData.
    COutputCoinData coutputCoinData;
    coutputCoinData.address = "st19m57r6grs3vwmx2el5dxuv3rdf4jjjx7tvsd4a9mrj4ezlphhaaq38wmfgt24dsmzttuntcsfjkekwd4g3ktyctj6tq2cgn2mu53df8kjyj9rstuvc78030ewugqgymvk7jf5lqgek373";
    coutputCoinData.value = 123;
    coutputCoinData.memo = "Foo";

    // Convert the COutputCoinData to a OutputCoinData.
    OutputCoinData outputCoinData = fromFFI(coutputCoinData);

    // Compare the two structs.
    //BOOST_CHECK_EQUAL(outputCoinData.address, coutputCoinData.address);
    BOOST_CHECK_EQUAL(outputCoinData.v, coutputCoinData.value);
    BOOST_CHECK_EQUAL(outputCoinData.memo, coutputCoinData.memo);

    // Print some information comparing the COutputCoinData and OutputCoinData.
    std::cout << std::endl << "COutputCoinData->OutputCoinData fromFFI debugging messages:" << std::endl;
    std::cout << "COutputCoinData address: " << coutputCoinData.address << std::endl;
    std::cout << "OutputCoinData  address: " << outputCoinData.address.encode(ADDRESS_NETWORK_TESTNET) << std::endl;
    std::cout << "COutputCoinData value: " << coutputCoinData.value << std::endl;
    std::cout << "OutputCoinData  value: " << outputCoinData.v << std::endl;
    std::cout << "COutputCoinData memo: " << coutputCoinData.memo << std::endl;
    std::cout << "OutputCoinData  memo: " << outputCoinData.memo << std::endl;
}

/*
 * Debug function to develop a OutputCoinData->COutputCoinData toFFI function.
 */
BOOST_AUTO_TEST_CASE(OutputCoinData_toFFI_test) {
    // Make a dummy OutputCoinData.
    OutputCoinData outputCoinData;
    spark::Address address;
    address.decode("st19m57r6grs3vwmx2el5dxuv3rdf4jjjx7tvsd4a9mrj4ezlphhaaq38wmfgt24dsmzttuntcsfjkekwd4g3ktyctj6tq2cgn2mu53df8kjyj9rstuvc78030ewugqgymvk7jf5lqgek373");
    outputCoinData.address = address;
    outputCoinData.v = 123;
    outputCoinData.memo = "Foo";

    // Convert the OutputCoinData to a COutputCoinData.
    COutputCoinData coutputCoinData = toFFI(outputCoinData);

    // Compare the two structs.
    //BOOST_CHECK_EQUAL(coutputCoinData.address, outputCoinData.address);
    BOOST_CHECK_EQUAL(coutputCoinData.value, outputCoinData.v);
    BOOST_CHECK_EQUAL(coutputCoinData.memo, outputCoinData.memo);

    // Print some information comparing the COutputCoinData and OutputCoinData.
    std::cout << std::endl << "OutputCoinData->COutputCoinData toFFI debugging messages:" << std::endl;
    std::cout << "OutputCoinData  address: " << outputCoinData.address.encode(spark::ADDRESS_NETWORK_TESTNET) << std::endl;
    std::cout << "COutputCoinData address: " << coutputCoinData.address << std::endl;
    std::cout << "OutputCoinData  value: " << outputCoinData.v << std::endl;
    std::cout << "COutputCoinData value: " << coutputCoinData.value << std::endl;
    std::cout << "OutputCoinData  memo: " << outputCoinData.memo << std::endl;
    std::cout << "COutputCoinData memo: " << coutputCoinData.memo << std::endl;
}

/*
 * Debug function to develop a CSparkMintMeta->CCSparkMintMeta fromFFI function.
 *
 * Not working, TODO fix.
 *
BOOST_AUTO_TEST_CASE(CCSparkMintMeta_fromFFI_test) {
    // Make a dummy CSparkMintMeta.
    uint64_t id = 123;
    std::string txidStr = "txid";
    const char* txid = txidStr.c_str();
    uint64_t diversifier = 456;
    std::string encryptedDiversifierStr = "diversifier";
    const char* encryptedDiversifier = encryptedDiversifierStr.c_str();
    uint64_t value = 789;
    std::string nonceStr = "nonce";
    const char* nonce = nonceStr.c_str();
    std::string memoStr = "memo";
    const char* memo = memoStr.c_str();
    std::vector<unsigned char> serialContextVec = {1, 2, 3};
    const unsigned char* serialContext = serialContextVec.data();
    int serialContextLength = static_cast<int>(serialContextVec.size());
    char type = 1; // Example type
    CCoin coin;

    std::cout << std::endl << "CCSparkMintMeta->CSparkMintMeta fromFFI debugging messages:" << std::endl;
    std::cout << "txidStr size: " << txidStr.size() << std::endl;
    std::cout << "encryptedDiversifierStr size: " << encryptedDiversifierStr.size() << std::endl;
    std::cout << "nonceStr size: " << nonceStr.size() << std::endl;
    std::cout << "memoStr size: " << memoStr.size() << std::endl;
    std::cout << "serialContext: " << serialContext << std::endl;
    std::cout << "serialContextVec size: " << serialContextVec.size() << std::endl;

    // Construct the CSparkMintMeta using its factory.
    CSparkMintMeta csparkMintMeta = createCSparkMintMeta(123, id, 1, txid, diversifier, encryptedDiversifier, value, nonce, memo, serialContext, serialContextLength, type, coin);
    // This fails: `fatal error: in "spark_address_tests/CCSparkMintMeta_fromFFI_test": std::length_error: cannot create std::vector larger than max_size()`.

    //// Convert the CSparkMintMeta to a CCSparkMintMeta.
    //CCSparkMintMeta ccsparkMintMeta = fromFFI(csparkMintMeta);
    //
    //// Create a dummy CCSparkMintMeta using the factory.
    //CCSparkMintMeta expected = createCCSparkMintMeta(123, "id", 1, "txid", 1, "diversifier", 1, 1, "memo", "serial context", 1, 1);
    //
    //// Compare the two structs.
    //BOOST_CHECK_EQUAL(ccsparkMintMeta.height, expected.height);
    //// etc.
}
 *

/*
 * Debug function to develop a CCSparkMintMeta->CSparkMintMeta toFFI function.
 *
BOOST_AUTO_TEST_CASE(CCSparkMintMeta_toFFI_test) {
    // Make a dummy CCSparkMintMeta.
    CCSparkMintMeta ccsparkMintMeta;
    ccsparkMintMeta.height = 123;
    ccsparkMintMeta.id = "id";
    ccsparkMintMeta.type = 1;
    ccsparkMintMeta.txid = "txid";
    ccsparkMintMeta.i = 789;

    // Correctly setting the encrypted diversifier
    const char* encryptedDiversifier = "encryptedDiversifier";
    ccsparkMintMeta.d = reinterpret_cast<const unsigned char*>(encryptedDiversifier);
    ccsparkMintMeta.dLength = std::strlen(encryptedDiversifier);

    ccsparkMintMeta.v = 101112;

    // Correctly setting the nonce
    const char* nonce = "nonce";
    ccsparkMintMeta.k = reinterpret_cast<const unsigned char*>(nonce);
    ccsparkMintMeta.kLength = std::strlen(nonce);

    ccsparkMintMeta.memo = "memo";
    ccsparkMintMeta.memoLength = 4;

    // Allocating and setting the serial context
    const char* serialContextStr = "serialContext";
    auto serialContextLength = std::strlen(serialContextStr);
    unsigned char* serialContext = new unsigned char[serialContextLength];
    std::memcpy(serialContext, serialContextStr, serialContextLength);
    ccsparkMintMeta.serial_context = serialContext;
    ccsparkMintMeta.serial_contextLength = serialContextLength;

    // Setting the coin
    ccsparkMintMeta.coin = createCCoin(
        COIN_TYPE_MINT,
        reinterpret_cast<const unsigned char*>("k"),
        1,
        "keyData",
        1,
        1,
        reinterpret_cast<const unsigned char*>("memo"),
        1,
        serialContext,
        serialContextLength
    );

    // Convert the CCSparkMintMeta to a CSparkMintMeta.
    CSparkMintMeta csparkMintMeta = fromFFI(ccsparkMintMeta);

    // Print something from csparkMintMeta just to use it.
    std::cout << std::endl << "CCSparkMintMeta->CSparkMintMeta toFFI debugging messages:" << std::endl;
    std::cout << "CCSparkMintMeta height: " << ccsparkMintMeta.height << std::endl;

    // Cleanup allocated memory
    delete[] serialContext;
}
 */

BOOST_AUTO_TEST_SUITE_END()
}
