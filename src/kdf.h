#ifndef FIRO_SPARK_KDF_H
#define FIRO_SPARK_KDF_H
#include "openssl_util.h"
#include "util.h"

namespace spark {

class KDF {
public:
	KDF(const std::string label, std::size_t derived_key_size);
	~KDF();
	KDF(const KDF&) = delete;
	KDF& operator=(const KDF&) = delete;
	void include(CDataStream& data);
	std::vector<unsigned char> finalize();

private:
	void include_size(std::size_t size);
	EVP_MD_CTX_ptr ctx;
	std::size_t derived_key_size;
};

}

#endif
