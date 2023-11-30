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
 * FFI-friendly wrapper for a spark::Coin.
 *
 * A Coin is a type, a key, an index, a value, a memo, and a serial context.  We accept these params
 * as a C struct, deriving the key from the keyData and index.
 *
 * TODO replace keyData and index with just an address which we decode.
 */
struct CCoin {
    char type;
    const unsigned char* k;
    int kLength;
    const char* address;
    uint64_t v;
    const unsigned char* memo;
    int memoLength;
    const unsigned char* serial_context;
    int serial_contextLength;
};

/*
 * FFI-friendly wrapper for a spark::IdentifiedCoinData.
 *
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
 * FFI-friendly wrapper for a spark::CRecipient.
 *
 * A CRecipient is a CScript, CAmount, and a bool.  We accept a C-style, FFI-friendly CCRecipient
 * struct in order to construct a C++ CRecipient.  A CScript is constructed from a hex string, a
 * CAmount is just a uint64_t, and the bool will be an int.
 */
struct CCRecipient {
    const unsigned char* pubKey;
    int pubKeyLength;
    uint64_t cAmount;
    int subtractFee;
};

/*
 * FFI-friendly wrapper for a spark::MintedCoinData.
 *
 * A MintedCoinData is a struct that contains an Address, a uint64_t value, and a string memo.  We
 * accept these as a CMintedCoinData from the Dart interface, and convert them to a MintedCoinData
 * struct.
 */
struct CMintedCoinData {
    const char* address;
    uint64_t value;
    const char* memo;
};

struct PubKeyScript {
    unsigned char* bytes;
    int length;
};

/*
 * FFI-friendly wrapper for spark::createSparkMintRecipients.
 */
EXPORT_DART
struct CCRecipient* createSparkMintRecipients(
    struct CMintedCoinData* outputs,
    int outputsLength,
    const char* serial_context,
    int serial_contextLength,
    int generate);

/*
 * FFI-friendly wrapper for a spark::OutputCoinData.
 *
 * An OutputCoinData is an address, value, and memo.  We accept these params as a C struct.
 */
struct COutputCoinData {
    const char* address;
    uint64_t value;
    const char* memo;
};

/*
 * FFI-friendly wrapper for a spark::CSparkMintMeta.
 *
 * A CSparkMintMeta is a struct that contains a height, id, isUsed, txid, diversifier, encrypted
 * diversifier, value, nonce, memo, serial context, type, and coin.  We accept these as a
 * CCSparkMintMeta from the Dart interface, and convert them to a C++ CSparkMintMeta struct.
 */
struct CCSparkMintMeta {
    uint64_t height;
    const char* id;
    int isUsed;
    const char* txid;
    uint64_t i; // Diversifier.
    const unsigned char* d; // Encrypted diversifier.
    int dLength;
    uint64_t v; // Value.
    const unsigned char* k; // Nonce.
    int kLength;
    const char* memo;
    int memoLength;
    unsigned char* serial_context;
    int serial_contextLength;
    char type;
    CDataStream coin;

    CCSparkMintMeta(uint64_t height, const char* id, int isUsed, const char* txid, uint64_t i, const unsigned char* d, int dLength, uint64_t v, const unsigned char* k, int kLength, const char* memo, int memoLength, unsigned char* serial_context, int serial_contextLength, char type, const CDataStream& coinData);
    ~CCSparkMintMeta();
};

/*
 * FFI-friendly wrapper for a spark::CoverSetData.
 */
struct CCoverSetData {
    CDataStream** cover_set; // vs. struct CCoin* cover_set;
    int cover_setLength;
    const unsigned char* cover_set_representation;
    int cover_set_representationLength;
};

#endif //ORG_FIRO_SPARK_DART_INTERFACE_H
