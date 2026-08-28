#ifndef FIRO_SPARK_OPENSSL_UTIL_H
#define FIRO_SPARK_OPENSSL_UTIL_H

#include <memory>
#include <stdexcept>

#include <openssl/evp.h>

namespace spark {

struct EVP_MD_CTX_Deleter {
    void operator()(EVP_MD_CTX* ctx) const { EVP_MD_CTX_free(ctx); }
};

struct EVP_CIPHER_CTX_Deleter {
    void operator()(EVP_CIPHER_CTX* ctx) const { EVP_CIPHER_CTX_free(ctx); }
};

using EVP_MD_CTX_ptr = std::unique_ptr<EVP_MD_CTX, EVP_MD_CTX_Deleter>;
using EVP_CIPHER_CTX_ptr = std::unique_ptr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_Deleter>;

inline EVP_MD_CTX_ptr MakeDigestContext()
{
    EVP_MD_CTX_ptr ctx(EVP_MD_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Unable to allocate OpenSSL digest context");
    }
    return ctx;
}

inline EVP_CIPHER_CTX_ptr MakeCipherContext()
{
    EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Unable to allocate OpenSSL cipher context");
    }
    return ctx;
}

inline void CheckOpenSSL(int result)
{
    if (result != 1) {
        throw std::runtime_error("OpenSSL operation failed");
    }
}

} // namespace spark

#endif
