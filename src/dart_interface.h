#ifndef ORG_FIRO_SPARK_DART_INTERFACE_H
#define ORG_FIRO_SPARK_DART_INTERFACE_H

#ifndef EXPORT_DART
    #ifdef __cplusplus
        #define EXPORT_DART extern "C" __attribute__((visibility("default"))) __attribute__((used))
    #elif
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

EXPORT_DART
CIdentifiedCoinData identifyCoin(const CCoin& c_struct, const char* keyDataHex, int index);

#endif //ORG_FIRO_SPARK_DART_INTERFACE_H
