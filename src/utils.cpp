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
#include "../bitcoin/script.h"  // For CScript.

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
        unsigned char* key_data_bin = hexToBytes(keyData);

        const SpendKeyData *data = new SpendKeyData(key_data_bin, index);

        return createSpendKey(*data);
    } catch (const std::exception& e) {
        // We can't return here, so just throw the exception again.
        throw e;
    }
}

/*
 * CCoin factory.
 *
 * TODO manage the memory allocated by this function.
 */
struct CCoin createCCoin(char type, const unsigned char* k, int kLength, const char* keyData, int index, uint64_t v, const unsigned char* memo, int memoLength, const unsigned char* serial_context, int serial_contextLength) {
	CCoin coin;
	coin.type = type;
	coin.k = copyBytes(k, kLength);
	coin.kLength = kLength;
	coin.keyData = strdup(keyData);
	coin.index = index;
	coin.v = v;
	coin.memo = copyBytes(memo, memoLength);
	coin.memoLength = memoLength;
	coin.serial_context = copyBytes(serial_context, serial_contextLength);
	coin.serial_contextLength = serial_contextLength;
	return coin;
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
	c_struct.d = copyBytes(cpp_struct.d.data(), cpp_struct.d.size());
	c_struct.dLength = cpp_struct.d.size();
	c_struct.v = cpp_struct.v;

	// Serialize and copy the Scalar k.
	std::vector<unsigned char> scalarBytes(32);
	cpp_struct.k.serialize(scalarBytes.data());
	c_struct.k = copyBytes(scalarBytes.data(), scalarBytes.size());
	c_struct.kLength = scalarBytes.size();

	// Copy the memo.
	c_struct.memo = strdup(cpp_struct.memo.c_str());
	c_struct.memoLength = cpp_struct.memo.size();

	return c_struct;
}

/*
 * Factory function to create a CScript from a byte array.
 */
CScript createCScriptFromBytes(const unsigned char* bytes, int length) {
	// Construct a CScript object
	CScript script;

	// Check if bytes is not nullptr and length is positive
	if (bytes != nullptr && length > 0) {
		// Append each byte to the script
		for (int i = 0; i < length; ++i) {
			script << bytes[i];
		}
	}

	return script;
}

/*
 * Utility function to convert a C++ CScript to a byte array.
 */
std::vector<unsigned char> serializeCScript(const CScript& script) {
	return std::vector<unsigned char>(script.begin(), script.end());
}

/*
 * Utility function to convert an FFI-friendly C CCRecipient struct to a C++ CRecipient struct.
 */
CRecipient fromFFI(const CCRecipient& c_struct) {
	// Use the factory function to create a CScript object.
	CScript script = createCScriptFromBytes(c_struct.pubKey, c_struct.pubKeyLength);

	CRecipient cpp_struct = createCRecipient(
			script,
			c_struct.cAmount,
			static_cast<bool>(c_struct.subtractFee)
	);

    return cpp_struct;
}

/*
 * CCRecipient factory.
 *
 * TODO manage the memory allocated by this function.
 */
struct CCRecipient createCCRecipient(const unsigned char* pubKey, uint64_t amount, int subtractFee) {
	CCRecipient recipient;
	recipient.pubKey = copyBytes(pubKey, 32);
	recipient.cAmount = amount;
	recipient.subtractFee = subtractFee;
	return recipient;
}

/*
 * Utility function to convert a C++ CRecipient struct to an FFI-friendly struct.
 */
CCRecipient toFFI(const CRecipient& cpp_struct) {
	CCRecipient c_struct;
	auto scriptBytes = serializeCScript(cpp_struct.pubKey);
	c_struct.pubKey = copyBytes(scriptBytes.data(), scriptBytes.size());
	c_struct.pubKeyLength = scriptBytes.size();
	c_struct.cAmount = cpp_struct.amount;
	c_struct.subtractFee = static_cast<int>(cpp_struct.subtractFeeFromAmount);
	return c_struct;
}

/*
 * CRecipient factory.
 *
 * TODO manage the memory allocated by this function.
 */
CRecipient createCRecipient(const CScript& script, CAmount amount, bool subtractFee) {
	CRecipient recipient;
	recipient.pubKey = script;
	recipient.amount = amount;
	recipient.subtractFeeFromAmount = subtractFee;
	return recipient;
}

/*
 * Utility function for deep copying byte arrays.
 *
 * Used by createCCoin.
 */
unsigned char* copyBytes(const unsigned char* source, int length) {
    if (source == nullptr || length <= 0) return nullptr;

    unsigned char* dest = new unsigned char[length];
    std::memcpy(dest, source, length);
    return dest;
}

unsigned char *hexToBytes(const char *hexstr) {
	size_t length = strlen(hexstr) / 2;
	auto *chrs = (unsigned char *) malloc((length + 1) * sizeof(unsigned char));
	for (size_t i = 0, j = 0; j < length; i += 2, j++) {
		chrs[j] = (hexstr[i] % 32 + 9) % 25 * 16 + (hexstr[i + 1] % 32 + 9) % 25;
	}
	chrs[length] = '\0';
	return chrs;
}

const char *bytesToHex(const unsigned char *bytes, int size) {
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

const char *bytesToHex(const char *bytes, int size) {
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

const char *bytesToHex(std::vector<unsigned char> bytes, int size) {
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
