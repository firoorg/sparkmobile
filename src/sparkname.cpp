#include "sparkname.h"

void GetSparkNameScript(spark::CSparkNameTxData& sparkNameData,
                        Scalar m,
                        const spark::SpendKey& spendKey,
                        const spark::IncomingViewKey& incomingViewKey,
                        std::vector<unsigned char>& outputScript)
{
    outputScript.clear();

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