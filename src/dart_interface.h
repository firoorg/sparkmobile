#ifndef ORG_FIRO_SPARK_DART_INTERFACE_H
#define ORG_FIRO_SPARK_DART_INTERFACE_H

#include <stdint.h>

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

/*
 * FFI-friendly wrapper for spark::getAddress.
 */
EXPORT_DART
const char* getAddress(const char* keyDataHex, int index, int diversifier, int isTestNet);

/*
EXPORT_DART
const char *createFullViewKey(const char* keyData, int index);

EXPORT_DART
const char* createIncomingViewKey(const char* keyData, int index);
*/

/*
 * A Coin is a type, a key, an index, a value, a memo, and a serial context.  We accept these params
 * as a C struct, deriving the key from the keyData and index.
 */
struct CCoin {
    char type;
    const unsigned char* k;
    int kLength;
    const char* keyData;
    int index;
    uint64_t v;
    const unsigned char* memo;
    int memoLength;
    const unsigned char* serial_context;
    int serial_contextLength;
};

/*
 * CCoin factory.
 */
EXPORT_DART
struct CCoin createCCoin(char type, const unsigned char* k, int kLength, const char* keyData, int index, uint64_t v, const unsigned char* memo, int memoLength, const unsigned char* serial_context, int serial_contextLength);

/*
 * An IdentifiedCoinData is a diversifier, encrypted diversifier, value, nonce, and memo.  We accept
 * these params as a C struct.
 */
struct CIdentifiedCoinData {
    uint64_t i;
    const unsigned char* d;
    int dLength;
    uint64_t v;
    const unsigned char* k;
    int kLength;
    const char* memo;
    int memoLength;
};

/*
 * FFI-friendly wrapper for spark::identifyCoin.
 */
EXPORT_DART
struct CIdentifiedCoinData identifyCoin(struct CCoin c_struct, const char* keyDataHex, int index);

/*
 * A CRecipient is a CScript, CAmount, and a bool.  We accept a C-style, FFI-friendly CCRecipient
 * struct in order to construct a C++ CRecipient.  A CScript is constructed from a hex string, a
 * CAmount is just a uint64_t, and the bool will be an int.
 */
struct CCRecipient {
    const unsigned char* cScript;
    int cScriptLength;
    uint64_t cAmount;
    int subtractFee;
};

#endif //ORG_FIRO_SPARK_DART_INTERFACE_H
