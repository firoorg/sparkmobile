#include "../src/sparkname.h"
#include "../include/spark.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>


class SparkTest {};

BOOST_FIXTURE_TEST_SUITE(spark_names_test, SparkTest)

BOOST_AUTO_TEST_CASE(spark_names)
{
    auto* params = spark::Params::get_default();

    Scalar m;
    m.randomize();

    spark::SpendKey spend_key(params, m);
    spark::FullViewKey full_view_key(spend_key);
    spark::IncomingViewKey incoming_view_key(full_view_key);

    std::vector<unsigned char> outputScript;

    spark::CSparkNameTxData sparkNameData;
    sparkNameData.name = "TestName";
    sparkNameData.sparkAddress = getAddress(incoming_view_key, 1234).encode(spark::ADDRESS_NETWORK_TESTNET);
    sparkNameData.sparkNameValidityBlocks = 2;
    sparkNameData.additionalInfo = "additional info";

    BOOST_CHECK_NO_THROW(GetSparkNameScript(sparkNameData, m, spend_key, incoming_view_key, outputScript));

    BOOST_CHECK(!outputScript.empty());

    CDataStream stream(outputScript, SER_NETWORK, PROTOCOL_VERSION);
    spark::CSparkNameTxData decodedData;
    BOOST_CHECK_NO_THROW(stream >> decodedData);

    BOOST_CHECK_EQUAL(decodedData.name, sparkNameData.name);
    BOOST_CHECK_EQUAL(decodedData.sparkAddress, sparkNameData.sparkAddress);
    BOOST_CHECK_EQUAL(decodedData.sparkNameValidityBlocks, sparkNameData.sparkNameValidityBlocks);
    BOOST_CHECK_EQUAL(decodedData.additionalInfo, sparkNameData.additionalInfo);
    BOOST_CHECK(!decodedData.addressOwnershipProof.empty());

    spark::OwnershipProof deserializedOwnershipProof;
    CDataStream deserializedStream(decodedData.addressOwnershipProof, SER_NETWORK, PROTOCOL_VERSION);
    deserializedStream >> deserializedOwnershipProof;

    spark::Address address(spark::Params::get_default());
    address.decode(decodedData.sparkAddress);

    BOOST_CHECK(address.verify_own(m, deserializedOwnershipProof));
}

BOOST_AUTO_TEST_SUITE_END()
