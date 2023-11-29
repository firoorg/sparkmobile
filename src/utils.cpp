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
struct CCoin createCCoin(char type, const unsigned char* k, int kLength, const char* address, uint64_t v, const unsigned char* memo, int memoLength, const unsigned char* serial_context, int serial_contextLength) {
	CCoin coin;
	coin.type = type;
	coin.k = copyBytes(k, kLength);
	coin.kLength = kLength;
	coin.address = address;
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
		decodeAddress(c_struct.address),
		c_struct.v,
		std::string(reinterpret_cast<const char*>(c_struct.memo), c_struct.memoLength),
		std::vector<unsigned char>(c_struct.serial_context, c_struct.serial_context + c_struct.serial_contextLength)
	);

	return cpp_struct;
}

/*
 * Utility function to convert a C++ Coin struct to an FFI-friendly C CDataStream struct.
 */
spark::Coin fromFFI(CDataStream& coinStream) {
	spark::Coin coin;
	coinStream >> coin;
	return coin;
}

/*
 * Utility function to convert a C++ Coin struct to an FFI-friendly C CDataStream struct.
 */
CDataStream toFFI(const spark::Coin& coin) {
    // Serialize the Coin object into a CDataStream
    CDataStream ccoinStream(SER_NETWORK, PROTOCOL_VERSION);
    ccoinStream << coin;

    return ccoinStream;
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

	// Serialize CScript and copy.
	std::vector<unsigned char> scriptBytes = serializeCScript(cpp_struct.pubKey);
	if (!scriptBytes.empty()) {
		c_struct.pubKey = copyBytes(scriptBytes.data(), scriptBytes.size());
		c_struct.pubKeyLength = static_cast<int>(scriptBytes.size());
	} else {
		c_struct.pubKey = nullptr;
		c_struct.pubKeyLength = 0;
	}

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
 * Utility function to decode an Address from a string.
 */
spark::Address decodeAddress(const std::string& str) {
	spark::Address address;
	address.decode(str);

	return address;
}

/*
 * MintedCoinData factory.
 */
spark::MintedCoinData createMintedCoinData(const char* address, uint64_t v, const char* memo) {
	return {
		decodeAddress(address),
		v,
		memo
	};
}

/*
 * Utility function to convert an FFI-friendly C CMintedCoinData struct to a C++ MintedCoinData.
 */
spark::MintedCoinData fromFFI(const CMintedCoinData& c_struct) {
	return createMintedCoinData(
		c_struct.address,
		c_struct.value,
		c_struct.memo
	);
}

/*
 * CMintedCoinData factory.
 */
CMintedCoinData createCMintedCoinData(const char* address, uint64_t value, const char* memo) {
	CMintedCoinData c_struct;
	c_struct.address = strdup(address);
	c_struct.value = value;
	c_struct.memo = strdup(memo);
	return c_struct;
}

/*
 * Utility function to convert a C++ MintedCoinData struct to an FFI-friendly CMintedCoinData.
 *
 * TODO add isTestNet flag for address encoding.
 */
CMintedCoinData toFFI(const spark::MintedCoinData& cpp_struct) {
    CMintedCoinData c_struct;
    c_struct.address = strdup(cpp_struct.address.encode(spark::ADDRESS_NETWORK_TESTNET).c_str());
    c_struct.value = cpp_struct.v;
    c_struct.memo = strdup(cpp_struct.memo.c_str());
    return c_struct;
}

/*
 * OutputCoinData factory.
 */
spark::OutputCoinData createOutputCoinData(const char* address, uint64_t v, const char* memo) {
	return {
		decodeAddress(address),
		v,
		memo
	};
}

/*
 * Utility function to convert an FFI-friendly C COutputCoinData struct to a C++ OutputCoinData.
 *
 * TODO add isTestNet flag for address encoding.
 */
spark::OutputCoinData fromFFI(const COutputCoinData& c_struct) {
	return createOutputCoinData(
		c_struct.address,
		c_struct.value,
		c_struct.memo
	);
}

/*
 * COutputCoinData factory.
 */
COutputCoinData createCOutputCoinData(const char* address, uint64_t value, const char* memo) {
	COutputCoinData c_struct;
	c_struct.address = strdup(address);
	c_struct.value = value;
	c_struct.memo = strdup(memo);
	return c_struct;
}

/*
 * Utility function to convert a C++ OutputCoinData struct to an FFI-friendly COutputCoinData.
 *
 * TODO add isTestNet flag for address encoding.
 */
COutputCoinData toFFI(const spark::OutputCoinData& cpp_struct) {
    // Encode address for testnet
    std::string address = cpp_struct.address.encode(spark::ADDRESS_NETWORK_TESTNET);

    return createCOutputCoinData(
		address.c_str(),
		cpp_struct.v,
		cpp_struct.memo.c_str()
	);
}

/*
 * CSparkMintMeta factory.
 *
 * A CSparkMintMeta is a C++ struct that contains a height, id, isUsed, txid, diversifier, encrypted
 * diversifier, value, nonce, memo, serial context, type, and coin.  We accept these as a
 * CCSparkMintMeta from the Dart interface and convert them to a C++ CSparkMintMeta struct.
 */
CSparkMintMeta createCSparkMintMeta(const uint64_t height, const uint64_t id, const int isUsed,
									const char* txidStr, const uint64_t diversifier,
									const char* encryptedDiversifierStr, const uint64_t value,
									const char* nonceStr, const char* memoStr,
									const unsigned char* serialContext,
									const int serialContextLength, const char type, const CCoin coin
) {
	CSparkMintMeta cpp_struct;

	cpp_struct.nHeight = height;
	cpp_struct.nId = id;
	cpp_struct.isUsed = isUsed != 0;

	if (txidStr) {
		cpp_struct.txid = uint256S(txidStr);
	}

	if (encryptedDiversifierStr) {
		size_t edLen = std::strlen(encryptedDiversifierStr);
		cpp_struct.d = std::vector<unsigned char>(encryptedDiversifierStr, encryptedDiversifierStr + edLen);
	}

	cpp_struct.i = diversifier;
	cpp_struct.v = value;

	if (nonceStr) {
		size_t nonceLen = std::strlen(nonceStr);
		std::vector<unsigned char> nonceBytes(nonceStr, nonceStr + nonceLen);
		cpp_struct.k = Scalar(nonceBytes.data());
	}

	if (memoStr) {
		cpp_struct.memo = std::string(memoStr);
	}

	if (serialContext && serialContextLength > 0) {
		cpp_struct.serial_context = std::vector<unsigned char>(serialContext, serialContext + serialContextLength);
	}

	cpp_struct.type = type;
	cpp_struct.coin = fromFFI(coin);

	return cpp_struct;
}

/*
 * Utility function to convert a C++ CSparkMintMeta struct to an FFI-friendly C CCSparkMintMeta.
 */
CSparkMintMeta fromFFI(const CCSparkMintMeta& c_struct) {
	CSparkMintMeta cpp_struct;
	cpp_struct.nHeight = c_struct.height;
	cpp_struct.nId = std::stoull(c_struct.id);
	cpp_struct.isUsed = c_struct.isUsed;
	cpp_struct.txid = uint256S(c_struct.txid);
	cpp_struct.i = c_struct.i;
	cpp_struct.d = std::vector<unsigned char>(c_struct.d, c_struct.d + c_struct.dLength);
	cpp_struct.v = c_struct.v;
	cpp_struct.k = bytesToScalar(c_struct.k, c_struct.kLength);
	cpp_struct.memo = std::string(c_struct.memo);
	cpp_struct.serial_context = std::vector<unsigned char>(c_struct.serial_context,
														   c_struct.serial_context +
														   c_struct.serial_contextLength);
	cpp_struct.type = c_struct.type;
	// c_struct.coin is a const, but we need a non-const reference to pass to fromFFI.
	// Convert c_struct.coin to a non-const.
	CDataStream coin = c_struct.coin;
	cpp_struct.coin = fromFFI(coin);
	return cpp_struct;
}

/*
 * CCSparkMintMeta constructor.
 *
 * Needed because CDataStream has no constructor.
 */
CCSparkMintMeta::CCSparkMintMeta(uint64_t height, const char* id, int isUsed, const char* txid,
								 uint64_t i, const unsigned char* d, int dLength, uint64_t v,
								 const unsigned char* k, int kLength, const char* memo,
								 int memoLength, unsigned char* serial_context,
								 int serial_contextLength, char type, const CDataStream& coinData
 ) : height(height), isUsed(isUsed), i(i), v(v), dLength(dLength), kLength(kLength),
     memoLength(memoLength), serial_contextLength(serial_contextLength), type(type), coin(coinData)
 {
	this->id = strdup(id);
	this->txid = strdup(txid);
	this->d = copyBytes(d, dLength);
	this->k = copyBytes(k, kLength);
	this->memo = strdup(memo);
	this->serial_context = copyBytes(serial_context, serial_contextLength);
}

/*
 * CCSparkMintMeta destructor.
 */
CCSparkMintMeta::~CCSparkMintMeta() {
	free(const_cast<char*>(id));
	free(const_cast<char*>(txid));
	delete[] d;
	delete[] k;
	free(const_cast<char*>(memo));
	delete[] serial_context;
}

/*
 * CCSparkMintMeta factory.
 *
 * A CSparkMintMeta is a struct that contains a height, id, isUsed, txid, diversifier, encrypted
 * diversifier, value, nonce, memo, serial context, type, and coin.  We accept these as a
 * CCSparkMintMeta from the Dart interface, and convert them to a C++ CSparkMintMeta struct.
 */
CCSparkMintMeta createCCSparkMintMeta(const uint64_t height, const uint64_t id, const int isUsed, const char* txid, const uint64_t diversifier, const char* encryptedDiversifier, const uint64_t value, const char* nonce, const char* memo, const unsigned char* serial_context, const int serial_contextLength, const char type, const CCoin coin) {
	// Create a string version of the id
	std::string idStr = std::to_string(id);
	const char* idCStr = idStr.c_str();

	// Convert encryptedDiversifier and nonce to unsigned char*
	auto encryptedDiversifierCStr = reinterpret_cast<const unsigned char*>(encryptedDiversifier);
	auto nonceCStr = reinterpret_cast<const unsigned char*>(nonce);

	// Convert coin to CDataStream
	spark::Coin coinStruct = fromFFI(coin);
	CDataStream coinStream = toFFI(coinStruct);

	// Construct CCSparkMintMeta using the constructor
	return CCSparkMintMeta(
		height,
		idCStr,
		isUsed,
		txid,
		diversifier,
		encryptedDiversifierCStr,
		std::strlen(encryptedDiversifier),
		value,
		nonceCStr,
		std::strlen(nonce),
		memo,
		std::strlen(memo),
		const_cast<unsigned char*>(serial_context),
		serial_contextLength,
		type,
		coinStream
	);
}

/*
 * Utility function to convert an FFI-friendly C CCSparkMintMeta struct to a C++ CSparkMintMeta.
 */
CCSparkMintMeta toFFI(const CSparkMintMeta& cpp_struct) {
	// Convert the id, txid, d, k, memo, and serial_context to appropriate types
	std::string idStr = std::to_string(cpp_struct.nId);
	const char* idCStr = idStr.c_str();

	CDataStream coinStream = toFFI(cpp_struct.coin);

	std::vector<unsigned char> scalarBytes(32);
	cpp_struct.k.serialize(scalarBytes.data());

	return CCSparkMintMeta(
		cpp_struct.nHeight,
		idCStr,
		cpp_struct.isUsed,
		cpp_struct.txid.ToString().c_str(),
		cpp_struct.i,
		copyBytes(cpp_struct.d.data(), cpp_struct.d.size()),
		cpp_struct.d.size(),
		cpp_struct.v,
		copyBytes(scalarBytes.data(), scalarBytes.size()), // Size should be 32.
		32,
		strdup(cpp_struct.memo.c_str()),
		cpp_struct.memo.size(),
		copyBytes(cpp_struct.serial_context.data(), cpp_struct.serial_context.size()),
		cpp_struct.serial_context.size(),
		cpp_struct.type,
		coinStream
	);
}

/*
 * CoverSetData factory.
 */
spark::CoverSetData createCoverSetData(
	const std::vector<spark::Coin>& cover_set,
   	const std::vector<unsigned char>& cover_set_representations
) {
	spark::CoverSetData coverSetData;
	coverSetData.cover_set = cover_set;
	coverSetData.cover_set_representation = cover_set_representations;
	return coverSetData;
}

/*
 * Utility function to convert an FFI-friendly C CCoverSetData struct to a C++ CoverSetData.
 */
spark::CoverSetData fromFFI(const CCoverSetData& c_struct) {
	std::vector<spark::Coin> cover_set;
	std::vector<std::vector<unsigned char>> cover_set_representation;

	for (int i = 0; i < c_struct.cover_setLength; i++) {
		spark::Coin coin;
		CDataStream coinStream = *c_struct.cover_set[i];
		coinStream >> coin;
		cover_set.emplace_back(coin);
		cover_set_representation.emplace_back(std::vector<unsigned char>(c_struct.cover_set_representation, c_struct.cover_set_representation + c_struct.cover_set_representationLength));
	}

	return createCoverSetData(cover_set, cover_set_representation); // This throws:
}

/*
 * CCoverSetData factory.
 */
CCoverSetData createCCoverSetData(const CCoin* cover_set,
								  const unsigned char* cover_set_representation,
								  const int cover_set_representationLength) {
	std::vector<CDataStream> cover_set_streams;

	for (int i = 0; i < cover_set_representationLength; i++) {
		// Convert CCoin to Coin.
		spark::Coin coin = fromFFI(cover_set[i]);

		// Serialize Coin.
		CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
		stream << coin;

		// Add stream to vector.
		cover_set_streams.push_back(stream);
	}

	// Create array containing the address of cover_set_streams
	CDataStream** cover_set_streams_pp = new CDataStream*[cover_set_representationLength];

	for (int i = 0; i < cover_set_representationLength; i++) {
		cover_set_streams_pp[i] = &cover_set_streams[i];
	}

	// Create the CCoverSetData and set the cover_set.
    CCoverSetData c_struct;

	// Assign the pointer to pointer
	c_struct.cover_set = cover_set_streams_pp;

	// Assign the cover_set_representation.
	c_struct.cover_set_representation = cover_set_representation;
	c_struct.cover_set_representationLength = cover_set_representationLength;
	return c_struct;
}

/*
 * Utility function to convert a C++ CoverSetData struct to an FFI-friendly CCoverSetData.
 *
 * We don't need toFFI yet (except to make complete tests...), so let's just comment it out.
 *
CCoverSetData toFFI(const spark::CoverSetData& cpp_struct) {
	CCoverSetData c_struct;
	// Converting the CCoins to spark::Coins.
	std::vector<spark::Coin> cover_set;
	for (int i = 0; i < cpp_struct.cover_set.size(); i++) {
		cover_set.push_back(fromFFI(cpp_struct.cover_set[i])); // This line throws.
	}

	// Serialize the spark::Coins as CDataStreams.
	std::vector<CDataStream> cover_set_streams;
	for (int i = 0; i < cover_set.size(); i++) {
		CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
		stream << cover_set[i];
		cover_set_streams.push_back(stream);
	}

	// Convert the std::vector<CDataStream> to a CDataStream*.
	CDataStream* cover_set_streams_array = &cover_set_streams[0];

	c_struct.cover_set_representation = cpp_struct.cover_set_representation.data();
	c_struct.cover_set_representationLength = cpp_struct.cover_set_representation.size();

	for (int i = 0; i < cpp_struct.cover_set.size(); i++) {
		c_struct.cover_set[i] = toFFI(cpp_struct.cover_set[i]);
	}

	return c_struct;
}
 */

unsigned char* copyBytes(const unsigned char* source, int length) {
    if (source == nullptr || length <= 0) return nullptr;

    unsigned char* dest = new unsigned char[length];
    std::memcpy(dest, source, length);
    return dest;
}

Scalar bytesToScalar(const unsigned char* bytes, int size) {
	if (bytes == nullptr || size <= 0) {
		throw std::invalid_argument("Invalid byte array for Scalar conversion.");
	}
	// Assuming Scalar can be constructed from a byte array.
	return Scalar(bytes);
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
