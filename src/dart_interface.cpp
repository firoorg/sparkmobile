#include "../include/spark.h"
#include "utils.h"
#include <cstring>
#include <iostream> // Just for printing.

using namespace spark;

/// Create a SpendKey from a given keyData and index.
///
/// keyData is derived using BIP44 from a seed.
const char * createSpendKey(const char *keyData, int index) {
    try {
        // Convert the keyData from hex string to binary
        unsigned char* key_data_bin = hex2bin(keyData);

        // Use the default (deployment) parameters.
        const spark::Params *params = spark::Params::get_default();

        // Generate r from keyData and index.
        std::string nCountStr = std::to_string(index);
        CHash256 hasher;
        std::string prefix = "r_generation";
        hasher.Write(reinterpret_cast<const unsigned char*>(prefix.c_str()), prefix.size());
        hasher.Write(key_data_bin, std::strlen(keyData) / 2); // divide by 2 because it's hex string (2 chars for 1 byte)
        hasher.Write(reinterpret_cast<const unsigned char*>(nCountStr.c_str()), nCountStr.size());
        unsigned char hash[CSHA256::OUTPUT_SIZE];
        hasher.Finalize(hash);

        // Create a Scalar from the hash.
        secp_primitives::Scalar r_scalar;
        r_scalar.memberFromSeed(hash);

        // Create a SpendKey from the Scalar.
        spark::SpendKey key(params, r_scalar);

        // Serialize the SpendKey or its components to an FFI-friendly format.
        // Assuming you need to serialize the Scalar r from the SpendKey,
        // and that the serialize method populates an unsigned char array.
        unsigned char serialized_r_buffer[32]; // Replace 32 if the size is different
        r_scalar.serialize(serialized_r_buffer);

        // Convert the serialized data to a hex string.
        char *serialized_r_str = bin2hex(serialized_r_buffer, sizeof(serialized_r_buffer));

        // Free the allocated memory for key_data_bin
        delete[] key_data_bin;

        // Return the hex string. Caller is responsible for freeing this memory.
        return serialized_r_str;

    } catch (const std::exception& e) {
        // If an exception is thrown, return a static error message.
        // Note: We are returning a string literal here because it is managed by the system
        // and doesn't need to be freed by the caller.
        return "Exception occurred";  // Or use e.what() if the caller can handle dynamic allocation.
    }
}

// Create a FullViewKey from a given SpendKey r.
const char * createFullViewKey(const char *spend_key_r) {
    try {
        // Use the default (deployment) parameters.
        const spark::Params *params = spark::Params::get_default();

        // Get the unsigned char* data from hex2bin.
        unsigned char* spend_key_data = hex2bin(spend_key_r);

        // Create a SpendKey from the spend key data.
        spark::SpendKey spend_key_(params, spend_key_data);

        // Create a FullViewKey from the SpendKey.
        spark::FullViewKey full_view_key(spend_key_);

        // Create a CDataStream object with the correct stream version and type.
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);

        // Serialize the full_view_key object into the stream.
        full_view_key.Serialize(ss);

        // Convert the stream to a std::vector<unsigned char>.
        std::vector<unsigned char> serialized_full_view_key(ss.begin(), ss.end());

        // Cast the serialized FullViewKey value to an FFI-friendly string.
        const char *serialized_full_view_key_str = bin2hex(serialized_full_view_key.data(), serialized_full_view_key.size());

        // Return the serialized FullViewKey value.
        return serialized_full_view_key_str;
    } catch (const std::exception& e) {
        // If an exception is thrown, print it to the console.
        std::cout << "Exception: " << e.what() << "\n";

        // Return the error message.
        return e.what();
    }
}
