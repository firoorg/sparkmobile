#include "../include/spark.h"
#include "utils.h"

#include <cstring>
#include <iostream> // Just for printing.

#ifndef EXPORT_DART
    #ifdef __cplusplus
        #define EXPORT_DART extern "C" __attribute__((visibility("default"))) __attribute__((used))
    #else
        #define EXPORT_DART __attribute__((visibility("default"))) __attribute__((used))
    #endif
    #ifdef _WIN32
        #define EXPORT_DART __declspec(dllexport)
    #endif
#endif

using namespace spark;

/*
 * FFI-friendly wrapper for spark::getAddress.
 *
 * Uses the utility function spark::createSpendKeyFromData(const char *keyData, int index) to pass
 * parameters to the C++ function spark::getAddress(const SpendKey& spendKey, uint64_t diversifier),
 * then uses the utility function std::string encode(const uint8_t network) to convert the result
 * back to a C string.
 *
 * getAddress: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/spark.cpp#L388
 */
EXPORT_DART
const char* getAddress(const char* keyDataHex, int index, int diversifier, int isTestNet) {
    try {
        // Use the hex string directly to create the SpendKey.
        spark::SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);

        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);
        spark::Address address(incomingViewKey, static_cast<uint64_t>(diversifier));

        // Encode the Address object into a string using the appropriate network.
        std::string encodedAddress = address.encode(isTestNet ? spark::ADDRESS_NETWORK_TESTNET : spark::ADDRESS_NETWORK_MAINNET);

        // Allocate memory for the C-style string.
        char* cstr = new char[encodedAddress.length() + 1];
        std::strcpy(cstr, encodedAddress.c_str());

        return cstr;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    }
}

/*
 * FFI-friendly wrapper for spark:identifyCoin.
 *
 * Uses the utility functions spark::Coin fromFFI(const CCoin& c_struct) to pass parameters to the
 * C++ function spark::identifyCoin(const Coin& coin), then uses the utility function
 * CIdentifiedCoinData toFFI(const spark::IdentifiedCoinData& cpp_struct) to convert the result back
 * to a C struct.
 *
 * We also need the incoming view key or CCoin we need to derive it, so accept keyDataHex and index.
 *
 * identifyCoin: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/spark.cpp#L400
 */
EXPORT_DART
struct CIdentifiedCoinData identifyCoin(struct CCoin c_struct, const char* keyDataHex, int index) {
    try {
        spark::Coin coin = fromFFI(c_struct);

        // Derive the incoming view key from the key data and index.
        spark::SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);
        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);

        spark::IdentifiedCoinData identifiedCoinData = coin.identify(incomingViewKey);
        return toFFI(identifiedCoinData);
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return CIdentifiedCoinData();
    }
}

/*
 * FFI-friendly wrapper for spark::createSparkMintRecipients.
 *
 * Uses the utility functions spark::MintedCoinData fromFFI(const CMintedCoinData& c_struct) to pass
 * parameters to the C++ function spark::createSparkMintRecipients, then uses the utility function
 * CRecipient toFFI(const spark::Recipient& cpp_struct) to convert the result back to a C struct.
 *
 * createSparkMintRecipients: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/spark.cpp#L43
 */
EXPORT_DART
struct CCRecipient* createSparkMintRecipients(
    struct CMintedCoinData* cOutputs,
    int outputsLength,
    const char* serial_context,
    int serial_contextLength,
    int generate
) {
    // Construct vector of spark::MintedCoinData.
    std::vector<spark::MintedCoinData> outputs;

    // Copy the data from the array to the vector.
    for (int i = 0; i < outputsLength; i++) {
        outputs.push_back(fromFFI(cOutputs[i]));
    }

    // Construct vector of unsigned chars.
    std::vector<unsigned char> blankSerialContext;

    // Copy the data from the array to the vector.
    for (int i = 0; i < serial_contextLength; i++) {
        blankSerialContext.push_back(serial_context[i]);
    }

    // Call spark::createSparkMintRecipients.
    std::vector<CRecipient> recipients = createSparkMintRecipients(outputs, blankSerialContext, generate);

    // Create a CRecipient* array.
    CCRecipient* cRecipients = new CCRecipient[recipients.size()];

    // Copy the data from the vector to the array.
    for (int i = 0; i < recipients.size(); i++) {
        cRecipients[i] = toFFI(recipients[i]);
    }

    // Return the array.
    return cRecipients;
}
