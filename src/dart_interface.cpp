#include "../include/spark.h"
#include "utils.h"

#include <cstring>
#include <iostream> // Just for printing.

using namespace spark;

/// FFI-friendly wrapper for spark:getAddress.
const char* getAddress(const char* keyData, int index, uint64_t diversifier) {
    try {
        // Convert keyData to Address object using getAddressFromData.
        spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);
        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);
        spark::Address address(incomingViewKey, diversifier);

        // Encode the Address object into a string.
        std::string encodedAddress = address.encode(ADDRESS_NETWORK_TESTNET);

        // Allocate memory for the C-style string.
        char* cstr = new char[encodedAddress.length() + 1];

        // Copy the std::string to the C-style string.
        std::strcpy(cstr, encodedAddress.c_str());

        return cstr;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    }
}

/*
/// FFI-friendly wrapper for spark::createFullViewKey.
const char* createFullViewKey(const char* keyData, int index) {
    try {
        spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);
        spark::FullViewKey fullViewKey(spendKey);

        // Serialize the FullViewKey.
        std::string serializedKey = serializeFullViewKey(fullViewKey);

        // Cast the string to an FFI-friendly char*.
        char* result = new char[serializedKey.size() + 1];
        std::copy(serializedKey.begin(), serializedKey.end(), result);
        result[serializedKey.size()] = '\0'; // Null-terminate the C string.

        return result;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

/// FFI-friendly wrapper for spark::createIncomingViewKey.
const char* createIncomingViewKey(const char* keyData, int index) {
    try {
        spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);
        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);

        // Serialize the FullViewKey.
        std::string serializedKey = serializeIncomingViewKey(incomingViewKey);

        // Cast the string to an FFI-friendly char*.
        char* result = new char[serializedKey.size() + 1];
        std::copy(serializedKey.begin(), serializedKey.end(), result);
        result[serializedKey.size()] = '\0'; // Null-terminate the C string.

        return result;
    } catch (const std::exception& e) {
        return nullptr;
    }
}
*/
