#include "../src/sparkname.h"
#include "../include/spark.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

namespace spark {

    class SparkTest {};

    BOOST_FIXTURE_TEST_SUITE(spark_names_test, SparkTest)

    BOOST_AUTO_TEST_CASE(spark_names)
    {
        auto* params = spark::Params::get_default();

        Scalar m;
        m.randomize();

        SpendKey spend_key(params, m);
        FullViewKey full_view_key(spend_key);
        IncomingViewKey incoming_view_key(full_view_key);

        std::vector<unsigned char> outputScript;
       
        CSparkNameTxData sparkNameData;
        sparkNameData.name = "TestName";
        sparkNameData.sparkAddress = getAddress(incoming_view_key, 1234).encode(ADDRESS_NETWORK_TESTNET);
        sparkNameData.sparkNameValidityBlocks = 2;
        sparkNameData.additionalInfo = "additional info";

        BOOST_CHECK_NO_THROW(GetSparkNameScript(sparkNameData, m, spend_key, incoming_view_key, outputScript));

        BOOST_CHECK(!outputScript.empty());

        CDataStream stream(outputScript, SER_NETWORK, PROTOCOL_VERSION);
        CSparkNameTxData decodedData;
        BOOST_CHECK_NO_THROW(stream >> decodedData);

        BOOST_CHECK_EQUAL(decodedData.name, sparkNameData.name);
        BOOST_CHECK_EQUAL(decodedData.sparkAddress, sparkNameData.sparkAddress);
        BOOST_CHECK_EQUAL(decodedData.sparkNameValidityBlocks, sparkNameData.sparkNameValidityBlocks);
        BOOST_CHECK_EQUAL(decodedData.additionalInfo, sparkNameData.additionalInfo);
        BOOST_CHECK(!decodedData.addressOwnershipProof.empty());

    }

    BOOST_AUTO_TEST_SUITE_END()
}
