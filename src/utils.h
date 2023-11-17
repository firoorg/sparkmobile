#ifndef ORG_FIRO_SPARK_UTILS_H
#define ORG_FIRO_SPARK_UTILS_H

#include "../include/spark.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char* getAddressFromData(const char* keyData, int index, const uint64_t diversifier);

spark::SpendKey createSpendKeyFromData(const char *keyData, int index);

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

spark::Coin fromFFI(const CCoin& c_struct);

CCoin toFFI(const spark::Coin& cpp_struct);

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

spark::IdentifiedCoinData fromFFI(const CIdentifiedCoinData& c_struct);

CIdentifiedCoinData toFFI(const spark::IdentifiedCoinData& cpp_struct);

char const hexArray[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
						   'e', 'f'};

unsigned char *hex2bin(const char *str);

const char *bin2hex(const unsigned char *bytes, int size);

const char *bin2hex(const char *bytes, int size);

const char *bin2hex(std::vector<unsigned char> bytes, int size);


#endif //ORG_FIRO_SPARK_UTILS_H
