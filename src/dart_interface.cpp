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

struct CCoin {
    const char type;
    const unsigned char* k;
    int kLength;
    const char* keyData;
    int index;
    uint64_t v;
    const unsigned char* memo;
    int memoLength;
    const unsigned char* serial_context;
    int serial_contextLength;

    // Constructor
    CCoin(char type,
          const unsigned char* k, int kLength,
          const char* keyData,
          int index,
          uint64_t v,
          const unsigned char* memo, int memoLength,
          const unsigned char* serial_context, int serial_contextLength)
            : type(type),
              k(copyBytes(k, kLength)), kLength(kLength),
              keyData(strdup(keyData)),
              index(index),
              v(v),
              memo(copyBytes(memo, memoLength)), memoLength(memoLength),
              serial_context(copyBytes(serial_context, serial_contextLength)),
              serial_contextLength(serial_contextLength) {}

    // Destructor
    ~CCoin() {
        delete[] k;
        delete[] keyData;
        delete[] memo;
        delete[] serial_context;
    }

private:
    // Helper function for deep copying byte arrays
    static unsigned char* copyBytes(const unsigned char* source, int length) {
        if (source == nullptr) return nullptr;
        unsigned char* dest = new unsigned char[length];
        std::memcpy(dest, source, length);
        return dest;
    }
};

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
CIdentifiedCoinData identifyCoin(const CCoin& c_struct, const char* keyDataHex, int index) {
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
