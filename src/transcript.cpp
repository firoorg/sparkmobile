#include "transcript.h"

namespace spark {

using namespace secp_primitives;

// Flags for transcript operations
const unsigned char FLAG_DOMAIN = 0;
const unsigned char FLAG_DATA = 1;
const unsigned char FLAG_VECTOR = 2;
const unsigned char FLAG_CHALLENGE = 3;

// Initialize a transcript with a domain separator
Transcript::Transcript(const std::string domain) : ctx(MakeDigestContext()) {
    CheckOpenSSL(EVP_DigestInit_ex(this->ctx.get(), EVP_sha512(), NULL));

    // Write the protocol and mode information
    std::vector<unsigned char> protocol(LABEL_PROTOCOL.begin(), LABEL_PROTOCOL.end());
    CheckOpenSSL(EVP_DigestUpdate(this->ctx.get(), protocol.data(), protocol.size()));
    CheckOpenSSL(EVP_DigestUpdate(this->ctx.get(), &HASH_MODE_TRANSCRIPT, sizeof(HASH_MODE_TRANSCRIPT)));

    // Domain separator
    include_flag(FLAG_DOMAIN);
    include_label(domain);
}

Transcript::~Transcript() = default;

Transcript& Transcript::operator=(const Transcript& t) {
    if (this == &t) {
        return *this;
    }

    auto copied_ctx = MakeDigestContext();
    CheckOpenSSL(EVP_MD_CTX_copy_ex(copied_ctx.get(), t.ctx.get()));
    this->ctx.swap(copied_ctx);

    return *this;
}

// Add a group element
void Transcript::add(const std::string label, const GroupElement& group_element) {
    std::vector<unsigned char> data;
    data.resize(GroupElement::serialize_size);
    group_element.serialize(data.data());

    include_flag(FLAG_DATA);
    include_label(label);
    include_data(data);
}

// Add a vector of group elements
void Transcript::add(const std::string label, const std::vector<GroupElement>& group_elements) {
    include_flag(FLAG_VECTOR);
    size(group_elements.size());
    include_label(label);
    for (std::size_t i = 0; i < group_elements.size(); i++) {
        std::vector<unsigned char> data;
        data.resize(GroupElement::serialize_size);
        group_elements[i].serialize(data.data());
        include_data(data);
    }
}

// Add a scalar
void Transcript::add(const std::string label, const Scalar& scalar) {
    std::vector<unsigned char> data;
    data.resize(SCALAR_ENCODING);
    scalar.serialize(data.data());

    include_flag(FLAG_DATA);
    include_label(label);
    include_data(data);
}

// Add a vector of scalars
void Transcript::add(const std::string label, const std::vector<Scalar>& scalars) {
    include_flag(FLAG_VECTOR);
    size(scalars.size());
    include_label(label);
    for (std::size_t i = 0; i < scalars.size(); i++) {
        std::vector<unsigned char> data;
        data.resize(SCALAR_ENCODING);
        scalars[i].serialize(data.data());
        include_data(data);
    }
}

// Add arbitrary data
void Transcript::add(const std::string label, const std::vector<unsigned char>& data) {
    include_flag(FLAG_DATA);
    include_label(label);
    include_data(data);
}

void Transcript::add(
        const std::string label,
        const std::vector<std::vector<unsigned char>>& data) {
    include_flag(FLAG_VECTOR);
    size(data.size());
    include_label(label);
    for (const auto& value : data) {
        include_data(value);
    }
}

// Produce a challenge
Scalar Transcript::challenge(const std::string label) {
    // Ensure we can properly populate a scalar
    if (EVP_MD_size(EVP_sha512()) < SCALAR_ENCODING) {
        throw std::runtime_error("Bad hash size!");
    }

    std::vector<unsigned char> hash;
    hash.resize(EVP_MD_size(EVP_sha512()));
    unsigned char counter = 0;

    auto state_counter = MakeDigestContext();
    auto state_finalize = MakeDigestContext();
    CheckOpenSSL(EVP_DigestInit_ex(state_counter.get(), EVP_sha512(), NULL));
    CheckOpenSSL(EVP_DigestInit_ex(state_finalize.get(), EVP_sha512(), NULL));

    include_flag(FLAG_CHALLENGE);
    include_label(label);

    while (1) {
        // Prepare temporary state for counter testing
        CheckOpenSSL(EVP_MD_CTX_copy_ex(state_counter.get(), this->ctx.get()));

        // Embed the counter
        CheckOpenSSL(EVP_DigestUpdate(state_counter.get(), &counter, sizeof(counter)));

        // Finalize the hash with a temporary state
        CheckOpenSSL(EVP_MD_CTX_copy_ex(state_finalize.get(), state_counter.get()));
        unsigned int TEMP; // We already know the digest length!
        CheckOpenSSL(EVP_DigestFinal_ex(state_finalize.get(), hash.data(), &TEMP));

        // Check for scalar validity
        Scalar candidate;
        try {
            candidate.deserialize(hash.data());
            this->ctx.swap(state_counter);

            return candidate;
        } catch (const std::exception &) {
            counter++;
        }
    }
}

// Encode and include a size
void Transcript::size(const std::size_t size_) {
    Scalar size_scalar(size_);
    std::vector<unsigned char> size_data;
    size_data.resize(SCALAR_ENCODING);
    size_scalar.serialize(size_data.data());
    CheckOpenSSL(EVP_DigestUpdate(this->ctx.get(), size_data.data(), size_data.size()));
}

// Include a flag
void Transcript::include_flag(const unsigned char flag) {
    CheckOpenSSL(EVP_DigestUpdate(this->ctx.get(), &flag, sizeof(flag)));
}

// Encode and include a label
void Transcript::include_label(const std::string label) {
    std::vector<unsigned char> bytes(label.begin(), label.end());
    include_data(bytes);
}

// Encode and include data
void Transcript::include_data(const std::vector<unsigned char>& data) {
    // Include size
    size(data.size());

    // Include data
    CheckOpenSSL(EVP_DigestUpdate(this->ctx.get(), data.data(), data.size()));
}

}
