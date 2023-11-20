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

spark::IdentifiedCoinData fromFFI(const CIdentifiedCoinData& c_struct);

CIdentifiedCoinData toFFI(const spark::IdentifiedCoinData& cpp_struct);

CRecipient fromFFI(const CCRecipient& c_struct);

CCRecipient toFFI(const CRecipient& cpp_struct);

char const hexArray[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
						   'e', 'f'};

unsigned char* copyBytes(const unsigned char* source, int length);

unsigned char *hexToBytes(const char *str);

const char *bytesToHex(const unsigned char *bytes, int size);

const char *bytesToHex(const char *bytes, int size);

const char *bytesToHex(std::vector<unsigned char> bytes, int size);


#endif //ORG_FIRO_SPARK_UTILS_H
