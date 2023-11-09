#include "../include/spark.h"
#include "utils.h"

#include <cstring>
#include <iostream> // Just for printing.

using namespace spark;
extern "C" {

/// FFI-friendly wrapper for spark:getAddress.
__attribute__((visibility("default"))) __attribute__((used))
const char* getAddress(const int* keyDataArray, int keyDataLength, int index, int diversifier) {
// To support a diversifier above 2,147,483,647, use the definition below.
// const char* getAddress(const int* keyDataArray, int index, int32_t diversifier_high, int32_t diversifier_low) {
    try {
        // Convert the keyDataArray to a vector of bytes (uint8_t).
        std::vector<uint8_t> keyData(keyDataArray, keyDataArray + keyDataLength);

        // To support a diversifier above 2,147,483,647, use the code below.
        // Combine the two 32-bit values into a single 64-bit unsigned integer.
        // uint64_t diversifier_cast = (static_cast<uint64_t>(diversifier_high) << 32) | (static_cast<uint32_t>(diversifier_low) & 0xFFFFFFFFULL);

        // Use the vector data to create the SpendKey.
        spark::SpendKey spendKey = createSpendKeyFromData(reinterpret_cast<const char*>(keyData.data()), index);
        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);
        spark::Address address(incomingViewKey, static_cast<uint64_t>(diversifier));

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

} // extern "C"
