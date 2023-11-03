#include "../include/spark.h"
#include "utils.h"
#include <cstring>
#include <iostream> // Just for printing.

using namespace spark;

/// Generate a new spend key and cast it to and FFI-friendly string.
const char * generateSpendKey() {
    try {
        // Use the default (deployment) parameters.
        const spark::Params *params = spark::Params::get_default();

        // Generate a new SpendKey.
        spark::SpendKey key(params);

        // Instead of just returning the SpendKey, we need to cast it to an FFI-friendly string.
        // The r value is used to create a SpendKey.  Use get_r to get the r value.
        const secp_primitives::Scalar &r = key.get_r();

        // Allocate a buffer of the required size.
        unsigned char serialized_r_buffer[32]; // Assuming size is 32, adjust if necessary

        // Pass this buffer to the r.serialize() method.
        r.serialize(serialized_r_buffer);

        // Fill the std::vector<unsigned char> with the serialized data.
        std::vector<unsigned char> serialized_r(serialized_r_buffer,
                                                serialized_r_buffer + sizeof(serialized_r_buffer));

        // Cast the serialized r value to an FFI-friendly string.
        const char *serialized_r_str = bin2hex(serialized_r.data(), serialized_r.size());

        // Return the serialized r value.
        return serialized_r_str;
    } catch (const std::exception& e) {
        // If an exception is thrown, print it to the console.
        std::cout << "Exception: " << e.what() << "\n";

        // Return the error message.
        return e.what();
    }
}

/// Create a SpendKey from a given r.
//extern "C" __attribute__((visibility("default"))) __attribute__((used))
const char * createSpendKey(const char *r) {
    try {
        // Use the default (deployment) parameters.
        const spark::Params *params = spark::Params::get_default();

        // Get the unsigned char* data from hex2bin.
        unsigned char* r_data = hex2bin(r);

        // Create a Scalar from the r data.
        secp_primitives::Scalar r_scalar(r_data);

        // Create a SpendKey from the r scalar.
        spark::SpendKey key(params, r_scalar);

        // Instead of just returning the SpendKey, we need to cast it to an FFI-friendly string.
        // The r value is used to create a SpendKey.  Use get_r to get the r value.
        const secp_primitives::Scalar &r = key.get_r();

        // Allocate a buffer of the required size.
        unsigned char serialized_r_buffer[32]; // Assuming size is 32, adjust if necessary

        // Pass this buffer to the r.serialize() method.
        r.serialize(serialized_r_buffer);

        // Fill the std::vector<unsigned char> with the serialized data.
        std::vector<unsigned char> serialized_r(serialized_r_buffer,
                                                serialized_r_buffer + sizeof(serialized_r_buffer));

        // Cast the serialized r value to an FFI-friendly string.
        const char *serialized_r_str = bin2hex(serialized_r.data(), serialized_r.size());

        // Return the serialized r value.
        return serialized_r_str;
    } catch (const std::exception& e) {
        // If an exception is thrown, print it to the console.
        std::cout << "Exception: " << e.what() << "\n";

        // Return the error message.
        return e.what();
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
