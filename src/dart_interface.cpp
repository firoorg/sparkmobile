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
 * We also need the incoming view key or we need to derive it, so accept keyDataHex and index.
 */
EXPORT_DART
struct CIdentifiedCoinData identifyCoin(struct CCoin ccoin, const char* keyDataHex, int index) {
    try {
        CDataStream ccoinStream(reinterpret_cast<const char*>(ccoin.k), reinterpret_cast<const char*>(ccoin.k) + ccoin.kLength);
        Coin coin;
        ccoinStream >> coin;

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
 */
EXPORT_DART
struct CCRecipient* createSparkMintRecipients(
        int numRecipients,
        struct PubKeyScript* pubKeyScripts,
        uint64_t* amounts,
        const char* memo,
        int subtractFee)
{
    try {
        std::vector<CRecipient> recipients;

        for (int i = 0; i < numRecipients; i++) {
            CScript scriptPubKey = createCScriptFromBytes(
                pubKeyScripts[i].bytes,
                pubKeyScripts[i].length
            );

            CRecipient recipient;
            recipient.pubKey = scriptPubKey;

            recipient.amount = amounts[i];

            recipient.subtractFeeFromAmount = (bool)subtractFee;

            recipients.push_back(recipient);
        }

        std::vector<CCRecipient> ccRecipients;

        for (const CRecipient& recipient : recipients) {
            CCRecipient ccRecipient = toFFI(recipient);
            ccRecipients.push_back(ccRecipient);
        }

        CCRecipient* result = new CCRecipient[numRecipients];
        std::copy(ccRecipients.begin(), ccRecipients.end(), result);

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    }
}
