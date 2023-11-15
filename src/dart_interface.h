#ifndef ORG_FIRO_SPARK_DART_INTERFACE_H
#define ORG_FIRO_SPARK_DART_INTERFACE_H

#include "stdint.h"

#define EXPORT_DART __attribute__((visibility("default"))) __attribute__((used))
#ifdef __cplusplus
    #define EXPORT_DART extern "C" __attribute__((visibility("default"))) __attribute__((used))
#endif
#ifdef _WIN32
    #define EXPORT_DART __declspec(dllexport)
#endif

EXPORT_DART
const char* getAddress(const char* keyDataHex, int index, uint64_t diversifier, int isTestNet);

/*
EXPORT_DART
const char *createFullViewKey(const char* keyData, int index);

EXPORT_DART
const char* createIncomingViewKey(const char* keyData, int index);
*/

#endif //ORG_FIRO_SPARK_DART_INTERFACE_H
