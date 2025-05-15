#ifndef FIRO_SPARK_NAME_H
#define FIRO_SPARK_NAME_H

#include "../bitcoin/amount.h"
#include "spark.h"

namespace spark {

struct CSparkNameTxData
{
public:
    static const uint16_t CURRENT_VERSION = 1;

public:
    uint16_t nVersion{CURRENT_VERSION};     // version
    uint256 inputsHash;

    // 1-20 symbols, only alphanumeric characters and hyphens
    std::string name;
    // destination address for the alias
    std::string sparkAddress;
    // proof of ownership of the spark address
    std::vector<unsigned char> addressOwnershipProof;
    // number of blocks the spark name is valid for
    uint32_t sparkNameValidityBlocks{0};
    // additional information, string, up to 1024 symbols. Can be used for future extensions (e.g. for storing a web link)
    std::string additionalInfo;
    // failsafe if the hash of the transaction data is can't be converted to a scalar for proof creation/verification
    uint32_t hashFailsafe{0};

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream &s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(inputsHash);
        READWRITE(name);
        READWRITE(sparkAddress);
        READWRITE(addressOwnershipProof);
        READWRITE(sparkNameValidityBlocks);
        READWRITE(additionalInfo);
        READWRITE(hashFailsafe);
    }
};

}

#endif // FIRO_SPARK_NAME_H
