#ifndef ORG_FIRO_SPARK_UTILS_H
#define ORG_FIRO_SPARK_UTILS_H

#include "../include/spark.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dart_interface.h"

const char* getAddressFromData(const char* keyData, int index, const uint64_t diversifier);

spark::SpendKey createSpendKeyFromData(const char *keyData, int index);

spark::Coin fromFFI(const CCoin& c_struct);

CCoin toFFI(const spark::Coin& cpp_struct);

struct CCoin createCCoin(char type, const unsigned char* k, int kLength, const char* keyData, int index, uint64_t v, const unsigned char* memo, int memoLength, const unsigned char* serial_context, int serial_contextLength);

spark::IdentifiedCoinData fromFFI(const CIdentifiedCoinData& c_struct);

CIdentifiedCoinData toFFI(const spark::IdentifiedCoinData& cpp_struct);

CScript createCScriptFromBytes(const unsigned char* bytes, int length);

std::vector<unsigned char> serializeCScript(const CScript& script);

CRecipient createCRecipient(const CScript& script, CAmount amount, bool subtractFee);

CRecipient fromFFI(const CCRecipient& c_struct);

struct CCRecipient createCCRecipient(const unsigned char* pubKey, uint64_t amount, int subtractFee);

CCRecipient toFFI(const CRecipient& cpp_struct);

char const hexArray[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
						   'e', 'f'};

unsigned char* copyBytes(const unsigned char* source, int length);

unsigned char *hexToBytes(const char *str);

const char *bytesToHex(const unsigned char *bytes, int size);

const char *bytesToHex(const char *bytes, int size);

const char *bytesToHex(std::vector<unsigned char> bytes, int size);


#endif //ORG_FIRO_SPARK_UTILS_H
