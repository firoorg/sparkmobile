#include "utils.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>
#include <string>
#include "dart_interface.h"
#include "coin.h"
#include "keys.h"

/*
 * Utility function to generate an address from keyData, index, and a diversifier.
 */
const char* getAddressFromData(const char* keyData, int index, const uint64_t diversifier) {
	try {
		spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);
		spark::FullViewKey fullViewKey(spendKey);
		spark::IncomingViewKey incomingViewKey(fullViewKey);
		spark::Address address(incomingViewKey, diversifier);

		// Encode the Address object into a string.
		std::string encodedAddress = address.encode(spark::ADDRESS_NETWORK_TESTNET);

		// Allocate memory for the C-style string and return it.
		char* result = new char[encodedAddress.size() + 1];
		std::copy(encodedAddress.begin(), encodedAddress.end(), result);
		result[encodedAddress.size()] = '\0'; // Null-terminate the C string.

		return result;
	} catch (const std::exception& e) {
		return nullptr;
	}
}

/*
 * Utility function to generate SpendKey from keyData and index.
 */
spark::SpendKey createSpendKeyFromData(const char *keyData, int index) {
    try {
        // Convert the keyData from hex string to binary
        unsigned char* key_data_bin = hex2bin(keyData);

        const SpendKeyData *data = new SpendKeyData(key_data_bin, index);

        return createSpendKey(*data);
    } catch (const std::exception& e) {
        // We can't return here, so just throw the exception again.
        throw e;
    }
}

/*
 * Utility function to convert an FFI-friendly C CCoin struct to a C++ Coin struct.
 */
spark::Coin fromFFI(const CCoin& c_struct) {
	spark::Coin cpp_struct(
		// The test params are only used for unit tests.
		spark::Params::get_default(),
		c_struct.type,
		spark::Scalar(c_struct.k),
		spark::Address(spark::IncomingViewKey(spark::FullViewKey(createSpendKeyFromData(c_struct.keyData, c_struct.index))), c_struct.index),
		c_struct.v,
		std::string(reinterpret_cast<const char*>(c_struct.memo), c_struct.memoLength),
		std::vector<unsigned char>(c_struct.serial_context, c_struct.serial_context + c_struct.serial_contextLength)
	);

	return cpp_struct;
}

/*
 * Utility function to convert a C++ IdentifiedCoinData struct to an FFI-friendly struct.
 */
CIdentifiedCoinData toFFI(const spark::IdentifiedCoinData& cpp_struct) {
	CIdentifiedCoinData c_struct;

	c_struct.i = cpp_struct.i;
	c_struct.d = cpp_struct.d.data();
	c_struct.dLength = cpp_struct.d.size();
	c_struct.v = cpp_struct.v;

	// Get the unsigned char* from the Scalar k using Scalar::serialize.
	std::vector<unsigned char> scalarBytes(32); // Scalar is typically 32 bytes.
	cpp_struct.k.serialize(scalarBytes.data());

	c_struct.k = scalarBytes.data();
	c_struct.memo = cpp_struct.memo.c_str();
	c_struct.memoLength = cpp_struct.memo.size();

	return c_struct;
}

unsigned char *hex2bin(const char *hexstr) {
	size_t length = strlen(hexstr) / 2;
	auto *chrs = (unsigned char *) malloc((length + 1) * sizeof(unsigned char));
	for (size_t i = 0, j = 0; j < length; i += 2, j++) {
		chrs[j] = (hexstr[i] % 32 + 9) % 25 * 16 + (hexstr[i + 1] % 32 + 9) % 25;
	}
	chrs[length] = '\0';
	return chrs;
}

const char *bin2hex(const unsigned char *bytes, int size) {
	std::string str;
	for (int i = 0; i < size; ++i) {
		const unsigned char ch = bytes[i];
		str.append(&hexArray[(ch & 0xF0) >> 4], 1);
		str.append(&hexArray[ch & 0xF], 1);
	}
    char *new_str = new char[std::strlen(str.c_str()) + 1];
    std::strcpy(new_str, str.c_str());
	return new_str;
}

const char *bin2hex(const char *bytes, int size) {
	std::string str;
	for (int i = 0; i < size; ++i) {
		const unsigned char ch = (const unsigned char) bytes[i];
		str.append(&hexArray[(ch & 0xF0) >> 4], 1);
		str.append(&hexArray[ch & 0xF], 1);
	}
    char *new_str = new char[std::strlen(str.c_str()) + 1];
    std::strcpy(new_str, str.c_str());
	return new_str;
}

const char *bin2hex(std::vector<unsigned char> bytes, int size) {
	std::string str;
	for (int i = 0; i < size; ++i) {
		const unsigned char ch = bytes[i];
		str.append(&hexArray[(ch & 0xF0) >> 4], 1);
		str.append(&hexArray[ch & 0xF], 1);
	}
    char *new_str = new char[std::strlen(str.c_str()) + 1];
    std::strcpy(new_str, str.c_str());
	return new_str;
}
