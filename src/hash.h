#ifndef FIRO_SPARK_HASH_H
#define FIRO_SPARK_HASH_H
#include "openssl_util.h"
#include "util.h"

namespace spark {

using namespace secp_primitives;

class Hash {
public:
	Hash(const std::string label);
	~Hash();
	Hash(const Hash&) = delete;
	Hash& operator=(const Hash&) = delete;
	void include(CDataStream& data);
	std::vector<unsigned char> finalize();
	Scalar finalize_scalar();
	GroupElement finalize_group();

private:
	void include_size(std::size_t size);
	EVP_MD_CTX_ptr ctx;
};

}

#endif
