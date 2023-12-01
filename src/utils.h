#ifndef ORG_FIRO_SPARK_UTILS_H
#define ORG_FIRO_SPARK_UTILS_H

#include "../include/spark.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dart_interface.h"

const char* getAddressFromData(const char* keyData, int index, const uint64_t diversifier, int isTestNet);

spark::SpendKey createSpendKeyFromData(const char *keyData, int index);

spark::Coin fromFFI(const CCoin& c_struct);

spark::Coin fromFFI(CDataStream& coinStream);

struct CCoin createCCoin(char type, const unsigned char* k, int kLength, const char* address, uint64_t v, const unsigned char* memo, int memoLength, const unsigned char* serial_context, int serial_contextLength);

CDataStream toFFI(const spark::Coin& cpp_struct);

spark::IdentifiedCoinData fromFFI(const CIdentifiedCoinData& c_struct);

CIdentifiedCoinData toFFI(const spark::IdentifiedCoinData& cpp_struct);

CScript createCScriptFromBytes(const unsigned char* bytes, int length);

std::vector<unsigned char> serializeCScript(const CScript& script);

CRecipient createCRecipient(const CScript& script, CAmount amount, bool subtractFee);

CRecipient fromFFI(const CCRecipient& c_struct);

struct CCRecipient createCCRecipient(const unsigned char* pubKey, uint64_t amount, int subtractFee);

CCRecipient toFFI(const CRecipient& cpp_struct);

spark::Address decodeAddress(const std::string& str);

spark::MintedCoinData createMintedCoinData(const char* address, uint64_t v, const char* memo);

spark::MintedCoinData fromFFI(const CMintedCoinData& c_struct);

CMintedCoinData createCMintedCoinData(const char* address, uint64_t value, const char* memo);

CMintedCoinData toFFI(const spark::MintedCoinData& cpp_struct, int isTestNet);

spark::OutputCoinData fromFFI(const COutputCoinData& c_struct);

spark::OutputCoinData createOutputCoinData(const char* address, uint64_t v, const char* memo);

COutputCoinData createCOutputCoinData(const char* address, uint64_t value, const char* memo);

COutputCoinData toFFI(const spark::OutputCoinData& cpp_struct, int isTestNet);

CSparkMintMeta createCSparkMintMeta(const uint64_t height, const uint64_t id, const int isUsed, const char* txid, const uint64_t diversifier, const char* encryptedDiversifier, const uint64_t value, const char* nonce, const char* memo, const unsigned char* serialContext, const int serialContextLength, const char type, const CCoin coin);

CSparkMintMeta fromFFI(const CCSparkMintMeta& c_struct);

CCSparkMintMeta createCCSparkMintMeta(const uint64_t height, const uint64_t id, const int isUsed, const char* txid, const uint64_t diversifier, const char* encryptedDiversifier, const uint64_t value, const char* nonce, const char* memo, const unsigned char* serialContext, const int serialContextLength, const char type, const CCoin coin);

CCSparkMintMeta toFFI(const CSparkMintMeta& cpp_struct);

spark::CoverSetData createCoverSetData(const std::vector<spark::Coin>& cover_set, const std::vector<std::vector<unsigned char>>& cover_set_representations);

spark::CoverSetData fromFFI(const CCoverSetData& c_struct);

CCoverSetData createCCoverSetData(const CCoin* cover_set, const unsigned char* cover_set_representation, const int cover_set_representationLength);

char const hexArray[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
						   'e', 'f'};

unsigned char* copyBytes(const unsigned char* source, int length);

Scalar bytesToScalar(const unsigned char* bytes, int size);

unsigned char *hexToBytes(const char *str);

const char *bytesToHex(const unsigned char *bytes, int size);

const char *bytesToHex(const char *bytes, int size);

const char *bytesToHex(std::vector<unsigned char> bytes, int size);


#endif //ORG_FIRO_SPARK_UTILS_H
