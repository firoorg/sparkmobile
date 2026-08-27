#include "../include/spark.h"
#include "../bitcoin/hash.h"

void GetSparkNameScript(spark::CSparkNameTxData& sparkNameData,
                        const uint256& ownershipDigest,
                        spark::SpendTransactionVersion version,
                        const spark::SpendKey& spendKey,
                        const spark::IncomingViewKey& incomingViewKey,
                        std::vector<unsigned char>& outputScript)
{
    outputScript.clear();
    const Scalar m = getSparkNameOwnershipMessage(ownershipDigest, version);

    spark::Address sparkAddress(spark::Params::get_default());
    spark::OwnershipProof ownershipProof;

    sparkAddress.decode(sparkNameData.sparkAddress);
    sparkAddress.prove_own(m, spendKey, incomingViewKey, ownershipProof);

    CDataStream ownershipProofStream(SER_NETWORK, PROTOCOL_VERSION);
    ownershipProofStream << ownershipProof;

    sparkNameData.addressOwnershipProof.assign(ownershipProofStream.begin(), ownershipProofStream.end());

    CDataStream sparkNameDataStream(SER_NETWORK, PROTOCOL_VERSION);
    sparkNameDataStream << sparkNameData;

    outputScript.insert(outputScript.end(), sparkNameDataStream.begin(), sparkNameDataStream.end());
}

size_t getSparkNameTxDataSize(const spark::CSparkNameTxData &sparkNameData)
{
    spark::CSparkNameTxData sparkNameDataCopy = sparkNameData;
    spark::OwnershipProof ownershipProof;   // just an empty proof

    CDataStream ownershipProofStream(SER_NETWORK, PROTOCOL_VERSION);
    ownershipProofStream << ownershipProof;

    sparkNameDataCopy.addressOwnershipProof.assign(ownershipProofStream.begin(), ownershipProofStream.end());

    CDataStream sparkNameDataStream(SER_NETWORK, PROTOCOL_VERSION);
    sparkNameDataStream << sparkNameDataCopy;

    return sparkNameDataStream.size();
}

uint256 getSparkNameCommitment(const spark::CSparkNameTxData &sparkNameData)
{
    spark::CSparkNameTxData committed = sparkNameData;
    committed.addressOwnershipProof.clear();

    CHashWriter hash(SER_GETHASH, PROTOCOL_VERSION);
    hash << std::string("FiroSparkNameExtensionV1") << committed;
    return hash.GetHash();
}

Scalar getSparkNameOwnershipMessage(
        const uint256& digest,
        spark::SpendTransactionVersion version)
{
    Scalar message;
    if (version == spark::SpendTransactionVersion::V1) {
        message.SetHex(digest.ToString());
        return message;
    }
    if (version != spark::SpendTransactionVersion::V2)
        throw std::invalid_argument("Unsupported Spark spend version");

    CHashWriter domain(SER_GETHASH, PROTOCOL_VERSION);
    domain << std::string("SparkNameOwnershipMessageV2") << digest;
    uint256 seed = domain.GetHash();
    message.memberFromSeed(seed.begin());
    return message;
}
