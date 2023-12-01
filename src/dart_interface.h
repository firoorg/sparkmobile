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
 * Coin: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/coin.h#L66
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
 * IdentifiedCoinData: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/coin.h#L19
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
 *
 * identifyCoin: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/spark.cpp#L400
 */
EXPORT_DART
struct CIdentifiedCoinData identifyCoin(struct CCoin c_struct, const char* keyDataHex, int index);

/*
 * FFI-friendly wrapper for a spark::CRecipient.
 *
 * CRecipient: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/include/spark.h#L27
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
 * MintedCoinData: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/mint_transaction.h#L12
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
 *
 * createSparkMintRecipients: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/spark.cpp#L43
 */
EXPORT_DART
struct CCRecipient* createSparkMintRecipients(
    struct CMintedCoinData* outputs,
    int outputsLength,
    const char* serial_context,
    int serial_contextLength,
    int generate);

/*
 * FFI-friendly wrapper for a std::pair<CAmount, bool>.
 *
 * Note this is an ambiguation of a spark::CRecipient.  This CRecip(ient) is just a wrapper for a
 * CAmount and bool pair, and is not the same as the spark::CRecipient struct above, which gets
 * wrapped for us as a CCRecipient and is unrelated to this struct.
 *
 * See https://github.com/firoorg/sparkmobile/blob/23099b0d9010a970ad75b9cfe05d568d634088f3/src/spark.cpp#L190
 */
struct CRecip {
    uint64_t amount;
    int subtractFee;
};

/*
 * FFI-friendly wrapper for a spark::OutputCoinData.
 *
 * OutputCoinData: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/spend_transaction.h#L33
 */
struct COutputCoinData {
    const char* address;
    uint64_t value;
    const char* memo;
};

/*
 * FFI-friendly wrapper for a <spark::OutputCoinData, bool>.
 *
 * See https://github.com/firoorg/sparkmobile/blob/23099b0d9010a970ad75b9cfe05d568d634088f3/src/spark.cpp#L195
 */
struct COutputRecipient {
    struct COutputCoinData output;
    int subtractFee;
};

/*
 * FFI-friendly wrapper for a spark::CSparkMintMeta.
 *
 * CSparkMintMeta: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/primitives.h#L9
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
 *
 * CoverSetData: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/spend_transaction.h#L28
 */
struct CCoverSetData {
    CDataStream** cover_set; // vs. struct CCoin* cover_set;
    int cover_setLength;
    const unsigned char* cover_set_representation;
    int cover_set_representationLength;
};

/*
 * FFI-friendly wrapper for a std::unordered_map<uint64_t, spark::CoverSetData>.
 *
 * See https://github.com/firoorg/sparkmobile/blob/23099b0d9010a970ad75b9cfe05d568d634088f3/src/spark.cpp#L197
 */
struct CCoverSets {
    struct CCoverSetData* cover_sets;
    int cover_setsLength;
};

struct OutputScript {
    unsigned char* bytes;
    int length;
};

/*
 * FFI-friendly wrapper for spark::createSparkSpendTransaction.
 *
 * createSparkSpendTransaction: https://github.com/firoorg/sparkmobile/blob/23099b0d9010a970ad75b9cfe05d568d634088f3/src/spark.cpp#L190
 */
EXPORT_DART
unsigned char* cCreateSparkSpendTransaction(
    const char* keyDataHex,
    int index,
    struct CRecip* recipients,
    int recipientsLength,
    struct COutputRecipient* privateRecipients,
    int privateRecipientsLength,
    struct CCSparkMintMeta* coins,
    int coinsLength,
    struct CCoverSets* cover_set_data_all,
    int cover_set_data_allLength,
    const char* txHashSig,
    int txHashSigLength,
    uint64_t fee,
    const OutputScript* outputScripts,
    int outputScriptsLength
);

#endif //ORG_FIRO_SPARK_DART_INTERFACE_H
