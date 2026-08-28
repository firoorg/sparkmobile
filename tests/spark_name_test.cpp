#include "../src/sparkname.h"
#include "../include/spark.h"
#include "../bitcoin/hash.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>


class SparkTest {};

BOOST_FIXTURE_TEST_SUITE(spark_names_test, SparkTest)

BOOST_AUTO_TEST_CASE(spark_names)
{
    auto* params = spark::Params::get_default();

    Scalar r;
    r.randomize();

    spark::SpendKey spend_key(params, r);
    spark::FullViewKey full_view_key(spend_key);
    spark::IncomingViewKey incoming_view_key(full_view_key);

    std::vector<unsigned char> outputScript;

    spark::CSparkNameTxData sparkNameData;
    sparkNameData.name = "TestName";
    sparkNameData.sparkAddress = getAddress(incoming_view_key, 1234).encode(spark::ADDRESS_NETWORK_TESTNET);
    sparkNameData.sparkNameValidityBlocks = 2;
    sparkNameData.additionalInfo = "additional info";

    const uint256 digest = uint256S("01");
    CHashWriter ownershipHash(SER_GETHASH, PROTOCOL_VERSION);
    ownershipHash << std::string("SparkNameOwnershipMessageV2") << digest;
    uint256 seed = ownershipHash.GetHash();
    Scalar v2Message;
    v2Message.memberFromSeed(seed.begin());

    BOOST_CHECK_NO_THROW(GetSparkNameScript(
        sparkNameData,
        digest,
        spark::SpendTransactionVersion::V2,
        spend_key,
        incoming_view_key,
        outputScript));

    BOOST_CHECK(!outputScript.empty());

    CDataStream stream(outputScript, SER_NETWORK, PROTOCOL_VERSION);
    spark::CSparkNameTxData decodedData;
    BOOST_CHECK_NO_THROW(stream >> decodedData);

    BOOST_CHECK_EQUAL(decodedData.name, sparkNameData.name);
    BOOST_CHECK_EQUAL(decodedData.sparkAddress, sparkNameData.sparkAddress);
    BOOST_CHECK_EQUAL(decodedData.sparkNameValidityBlocks, sparkNameData.sparkNameValidityBlocks);
    BOOST_CHECK_EQUAL(decodedData.additionalInfo, sparkNameData.additionalInfo);
    BOOST_CHECK_EQUAL(decodedData.nVersion, uint16_t{2});
    BOOST_CHECK_EQUAL((int)decodedData.operationType, 0);
    BOOST_CHECK(!decodedData.addressOwnershipProof.empty());

    spark::OwnershipProof deserializedOwnershipProof;
    CDataStream deserializedStream(decodedData.addressOwnershipProof, SER_NETWORK, PROTOCOL_VERSION);
    deserializedStream >> deserializedOwnershipProof;

    spark::Address address(spark::Params::get_default());
    address.decode(decodedData.sparkAddress);

    BOOST_CHECK(address.verify_own(v2Message, deserializedOwnershipProof));

    BOOST_CHECK_NO_THROW(GetSparkNameScript(
        sparkNameData,
        digest,
        spark::SpendTransactionVersion::V1,
        spend_key,
        incoming_view_key,
        outputScript));
    CDataStream v1Stream(outputScript, SER_NETWORK, PROTOCOL_VERSION);
    v1Stream >> decodedData;
    CDataStream v1ProofStream(
        decodedData.addressOwnershipProof, SER_NETWORK, PROTOCOL_VERSION);
    v1ProofStream >> deserializedOwnershipProof;
    Scalar v1Message;
    v1Message.SetHex(digest.ToString());
    BOOST_CHECK(address.verify_own(v1Message, deserializedOwnershipProof));

    outputScript = {1};
    BOOST_CHECK_THROW(
        GetSparkNameScript(
            sparkNameData,
            digest,
            static_cast<spark::SpendTransactionVersion>(3),
            spend_key,
            incoming_view_key,
            outputScript),
        std::invalid_argument);
    BOOST_CHECK(outputScript.empty());
}

BOOST_AUTO_TEST_CASE(spark_name_v2_binding_helpers)
{
    spark::CSparkNameTxData data;
    data.inputsHash = uint256S("10");
    data.name = "TestName";
    data.sparkAddress = "test-address";
    data.addressOwnershipProof = {1, 2, 3};
    data.sparkNameValidityBlocks = 2;
    data.additionalInfo = "additional info";

    spark::CSparkNameTxData committed = data;
    committed.addressOwnershipProof.clear();
    CHashWriter commitmentHash(SER_GETHASH, PROTOCOL_VERSION);
    commitmentHash << std::string("FiroSparkNameExtensionV1") << committed;
    const uint256 commitment = getSparkNameCommitment(data);
    BOOST_CHECK(commitment == commitmentHash.GetHash());

    data.addressOwnershipProof = {4, 5, 6};
    BOOST_CHECK(getSparkNameCommitment(data) == commitment);

    const uint256 digest = uint256S("20");
    CHashWriter ownershipHash(SER_GETHASH, PROTOCOL_VERSION);
    ownershipHash << std::string("SparkNameOwnershipMessageV2") << digest;
    uint256 seed = ownershipHash.GetHash();
    Scalar expectedV2;
    expectedV2.memberFromSeed(seed.begin());
    BOOST_CHECK(
        getSparkNameOwnershipMessage(
            digest, spark::SpendTransactionVersion::V2) == expectedV2);

    Scalar expectedV1;
    expectedV1.SetHex(digest.ToString());
    BOOST_CHECK(
        getSparkNameOwnershipMessage(
            digest, spark::SpendTransactionVersion::V1) == expectedV1);
    BOOST_CHECK_THROW(
        getSparkNameOwnershipMessage(
            uint256S(std::string(64, 'f')),
            spark::SpendTransactionVersion::V1),
        std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rejects_unsupported_spark_name_data)
{
    spark::CSparkNameTxData data;
    data.nVersion = 3;
    CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
    BOOST_CHECK_THROW(serialized << data, std::ios_base::failure);

    data.nVersion = 2;
    CDataStream encodedData(SER_NETWORK, PROTOCOL_VERSION);
    encodedData << data;
    encodedData[0] = 3;
    encodedData[1] = 0;
    BOOST_CHECK_THROW(encodedData >> data, std::ios_base::failure);

    data.nVersion = 2;
    data.operationType = 1;
    CDataStream unsupportedOperation(SER_NETWORK, PROTOCOL_VERSION);
    BOOST_CHECK_THROW(unsupportedOperation << data, std::ios_base::failure);

    data.operationType = 0;
    CDataStream encodedOperation(SER_NETWORK, PROTOCOL_VERSION);
    encodedOperation << data;
    encodedOperation[encodedOperation.size() - 1] = 1;
    BOOST_CHECK_THROW(encodedOperation >> data, std::ios_base::failure);
}

BOOST_AUTO_TEST_SUITE_END()
