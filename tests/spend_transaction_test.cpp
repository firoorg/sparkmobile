#include "../src/spend_transaction.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

namespace spark {

using namespace secp_primitives;

// Generate a random char vector from a random scalar
static std::vector<unsigned char> random_char_vector() {
    Scalar temp;
    temp.randomize();
    std::vector<unsigned char> result;
    result.resize(SCALAR_ENCODING);
    temp.serialize(result.data());

    return result;
}

class SparkTest {};

BOOST_FIXTURE_TEST_SUITE(spark_spend_transaction_tests, SparkTest)

BOOST_AUTO_TEST_CASE(versioned_generate_verify_and_serialization)
{
    // Parameters
    const Params* params;
    params = Params::get_test();

    const std::string memo = "Spam and eggs"; // arbitrary memo

    // Generate keys
    SpendKey spend_key(params);
    FullViewKey full_view_key(spend_key);
    IncomingViewKey incoming_view_key(full_view_key);

    // Generate address
    const uint64_t i = 12345;
    Address address(incoming_view_key, i);

    // Mint some coins to the address
    std::size_t N = (std::size_t) pow(params->get_n_grootle(), params->get_m_grootle());
    std::vector<Coin> in_coins;
    for (std::size_t i = 0; i < N; i++) {
        Scalar k;
        k.randomize();

        uint64_t v = 123 + i; // arbitrary value

        in_coins.emplace_back(Coin(
            params,
            COIN_TYPE_MINT,
            k,
            address,
            v,
            memo,
            random_char_vector()
        ));
    }

    // Track values so we can set the fee to make the transaction balance
    uint64_t f = 0;

    // Choose coins to spend, recover them, and prepare them for spending
    std::vector<std::size_t> spend_indices = { 1, 3, 5 };
    std::vector<InputCoinData> spend_coin_data;
    std::unordered_map<uint64_t, CoverSetData> cover_set_data;
    const std::size_t w = spend_indices.size();
    const std::vector<unsigned char> shared_cover_set_representation =
        random_char_vector();
    for (std::size_t u = 0; u < w; u++) {
        IdentifiedCoinData identified_coin_data = in_coins[spend_indices[u]].identify(incoming_view_key);
        RecoveredCoinData recovered_coin_data = in_coins[spend_indices[u]].recover(full_view_key, identified_coin_data);

        spend_coin_data.emplace_back();
        uint64_t cover_set_id = 31415 + (u % 2);
        spend_coin_data.back().cover_set_id = cover_set_id;

        CoverSetData setData;
        setData.cover_set = in_coins;
        setData.cover_set_representation = shared_cover_set_representation;
        cover_set_data[cover_set_id] = setData;
        spend_coin_data.back().index = spend_indices[u];
        spend_coin_data.back().k = identified_coin_data.k;
        spend_coin_data.back().s = recovered_coin_data.s;
        spend_coin_data.back().T = recovered_coin_data.T;
        spend_coin_data.back().v = identified_coin_data.v;

        f += identified_coin_data.v;
    }

    // Generate new output coins and compute the fee
    const std::size_t t = 3;
    std::vector<OutputCoinData> out_coin_data;
    for (std::size_t j = 0; j < t; j++) {
        out_coin_data.emplace_back();
        out_coin_data.back().address = address;
        out_coin_data.back().v = 12 + j; // arbitrary value
        out_coin_data.back().memo = memo;

        f -= out_coin_data.back().v;
    }

    // Assert the fee is correct
    uint64_t fee_test = f;
    for (std::size_t j = 0; j < t; j++) {
        fee_test += out_coin_data[j].v;
    }
    for (std::size_t u = 0; u < w; u++) {
        fee_test -= spend_coin_data[u].v;
    }

    if (fee_test != 0) {
        throw std::runtime_error("Bad fee assertion");
    }

    // Generate spend transaction
    SpendTransaction transaction(
        params,
        full_view_key,
        spend_key,
        spend_coin_data,
        cover_set_data,
        f,
        0,
        out_coin_data
    );

    BOOST_CHECK_THROW(
        SpendTransaction(
            params,
            full_view_key,
            spend_key,
            spend_coin_data,
            cover_set_data,
            f,
            0,
            out_coin_data,
            SpendTransactionVersion::V1,
            uint256S("01")),
        std::invalid_argument);

    // Historical V1 remains byte-compatible and multi-input verification is
    // available only through the explicit historical path.
    transaction.setCoverSets(cover_set_data);
    std::unordered_map<uint64_t, std::vector<Coin>> cover_sets;
    for (const auto set_data : cover_set_data)
        cover_sets[set_data.first] = set_data.second.cover_set;
    BOOST_REQUIRE(SpendTransaction::verifyHistorical(transaction, cover_sets));
    BOOST_CHECK(!SpendTransaction::verify(transaction, cover_sets));

    CDataStream encodedV1(SER_NETWORK, PROTOCOL_VERSION);
    encodedV1 << transaction;
    BOOST_REQUIRE(!encodedV1.empty());
    BOOST_CHECK_EQUAL(static_cast<unsigned char>(encodedV1[0]), w);

    std::map<uint64_t, uint256> blockHashes;
    for (const auto& entry : cover_set_data) {
        blockHashes.emplace(entry.first, uint256S("01"));
    }
    const auto checkInvalidConstructorGroupId = [&](uint64_t invalidId) {
        auto invalidInputs = spend_coin_data;
        invalidInputs[0].cover_set_id = invalidId;
        auto invalidCoverSets = cover_set_data;
        invalidCoverSets.emplace(
            invalidId,
            cover_set_data.at(spend_coin_data[0].cover_set_id));
        auto invalidBlockHashes = blockHashes;
        invalidBlockHashes.emplace(invalidId, uint256S("01"));
        BOOST_CHECK_EXCEPTION(
            SpendTransaction(
                params,
                full_view_key,
                spend_key,
                invalidInputs,
                invalidCoverSets,
                f,
                0,
                out_coin_data,
                SpendTransactionVersion::V2,
                uint256(),
                invalidBlockHashes),
            std::invalid_argument,
            [](const std::invalid_argument& error) {
                return std::string(error.what()) ==
                    "Bad Spark V2 cover-set id";
            });
    };
    checkInvalidConstructorGroupId(0);
    checkInvalidConstructorGroupId(
        static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) + 1);

    SpendTransaction transactionV2(
        params,
        full_view_key,
        spend_key,
        spend_coin_data,
        cover_set_data,
        f,
        0,
        out_coin_data,
        SpendTransactionVersion::V2,
        uint256S("01"),
        blockHashes);
    transactionV2.setCoverSets(cover_set_data);
    BOOST_REQUIRE(SpendTransaction::verify(transactionV2, cover_sets));

    SpendTransaction changedReferences(transactionV2);
    auto replacementReferences = blockHashes;
    replacementReferences.begin()->second = uint256S("02");
    changedReferences.setBlockHashes(replacementReferences);
    BOOST_CHECK(!SpendTransaction::verify(changedReferences, cover_sets));

    CDataStream encodedV2(SER_NETWORK, PROTOCOL_VERSION);
    encodedV2 << transactionV2;
    const std::vector<unsigned char> originalV2(
        encodedV2.begin(), encodedV2.end());
    BOOST_REQUIRE(!encodedV2.empty());
    BOOST_CHECK_EQUAL(static_cast<unsigned char>(encodedV2[0]), 2U);

    const std::size_t idsOffset = 1 + GetSizeOfCompactSize(w) +
        GetSizeOfCompactSize(t);
    const std::size_t mapOffset = idsOffset + w * sizeof(uint64_t) +
        GetSizeOfCompactSize(blockHashes.size());
    const std::size_t mapEntrySize = sizeof(uint64_t) + uint256().size();
    BOOST_REQUIRE(originalV2.size() >= mapOffset + 2 * mapEntrySize);
    const auto checkInvalidGroupId = [&](std::vector<unsigned char> bytes) {
        CDataStream invalid(bytes, SER_NETWORK, PROTOCOL_VERSION);
        SpendTransaction parser(
            params, SpendTransactionVersion::V2, out_coin_data.size());
        BOOST_CHECK_EXCEPTION(
            invalid >> parser,
            std::ios_base::failure,
            [](const std::ios_base::failure& error) {
                return std::string(error.what()).find(
                    "bad Spark V2 cover-set id") != std::string::npos;
            });
    };

    auto zeroGroupId = originalV2;
    WriteLE64(zeroGroupId.data() + idsOffset, 0);
    WriteLE64(zeroGroupId.data() + idsOffset + 2 * sizeof(uint64_t), 0);
    WriteLE64(zeroGroupId.data() + mapOffset, 0);
    checkInvalidGroupId(zeroGroupId);

    const uint64_t oversizedGroupId =
        static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) + 1;
    auto oversizedGroup = originalV2;
    WriteLE64(
        oversizedGroup.data() + idsOffset + sizeof(uint64_t),
        oversizedGroupId);
    WriteLE64(
        oversizedGroup.data() + mapOffset + mapEntrySize,
        oversizedGroupId);
    checkInvalidGroupId(oversizedGroup);

    SpendTransaction decodedV2(
        params, SpendTransactionVersion::V2, out_coin_data.size());
    encodedV2 >> decodedV2;
    BOOST_CHECK(encodedV2.empty());
    CDataStream canonicalV2(SER_NETWORK, PROTOCOL_VERSION);
    canonicalV2 << decodedV2;
    const std::vector<unsigned char> canonicalBytes(
        canonicalV2.begin(), canonicalV2.end());
    BOOST_CHECK(canonicalBytes == originalV2);
    decodedV2.setOutCoins(transactionV2.getOutCoins());
    decodedV2.setVout(0);
    decodedV2.setCoverSets(cover_set_data);
    BOOST_CHECK(SpendTransaction::verify(decodedV2, cover_sets));

    CDataStream reorderedIds(SER_NETWORK, PROTOCOL_VERSION);
    reorderedIds << transactionV2;
    BOOST_REQUIRE(reorderedIds.size() >= 19U);
    std::swap_ranges(
        reorderedIds.begin() + 3,
        reorderedIds.begin() + 11,
        reorderedIds.begin() + 11);
    SpendTransaction reorderedParser(
        params, SpendTransactionVersion::V2, out_coin_data.size());
    reorderedIds >> reorderedParser;
    BOOST_CHECK(reorderedIds.empty());
    reorderedParser.setOutCoins(transactionV2.getOutCoins());
    reorderedParser.setVout(0);
    reorderedParser.setCoverSets(cover_set_data);
    BOOST_CHECK(!SpendTransaction::verify(reorderedParser, cover_sets));

    SpendTransaction extraneousCoverSet(transactionV2);
    auto extraneousBlockHashes = blockHashes;
    extraneousBlockHashes.emplace(999, uint256S("02"));
    extraneousCoverSet.setBlockHashes(extraneousBlockHashes);
    CDataStream extraneousEncoding(SER_NETWORK, PROTOCOL_VERSION);
    BOOST_CHECK_EXCEPTION(
        extraneousEncoding << extraneousCoverSet,
        std::ios_base::failure,
        [](const std::ios_base::failure& error) {
            return std::string(error.what()).find(
                "Spark V2 cover-set hash has no input") != std::string::npos;
        });

    SpendTransaction missingCoverSet(transactionV2);
    missingCoverSet.setBlockHashes({});
    CDataStream missingEncoding(SER_NETWORK, PROTOCOL_VERSION);
    BOOST_CHECK_THROW(missingEncoding << missingCoverSet, std::exception);

    CDataStream truncatedV2(SER_NETWORK, PROTOCOL_VERSION);
    truncatedV2 << transactionV2;
    truncatedV2.resize(truncatedV2.size() - 1);
    SpendTransaction truncatedParser(
        params, SpendTransactionVersion::V2, out_coin_data.size());
    BOOST_CHECK_THROW(truncatedV2 >> truncatedParser, std::exception);

    SpendTransaction wrongV2Parser(
        params, SpendTransactionVersion::V2, out_coin_data.size());
    BOOST_CHECK_THROW(encodedV1 >> wrongV2Parser, std::exception);

    CDataStream wrongVersion(SER_NETWORK, PROTOCOL_VERSION);
    wrongVersion << transactionV2;
    wrongVersion[0] = 3;
    SpendTransaction rejectedVersion(
        params, SpendTransactionVersion::V2, out_coin_data.size());
    BOOST_CHECK_THROW(wrongVersion >> rejectedVersion, std::exception);

    CDataStream oversized(SER_NETWORK, PROTOCOL_VERSION);
    oversized << static_cast<uint8_t>(SpendTransactionVersion::V2);
    WriteCompactSize(oversized, MAX_CHAUM_V2_INPUTS + 1);
    WriteCompactSize(oversized, out_coin_data.size());
    SpendTransaction boundedParser(
        params, SpendTransactionVersion::V2, out_coin_data.size());
    BOOST_CHECK_EXCEPTION(
        oversized >> boundedParser,
        std::ios_base::failure,
        [](const std::ios_base::failure& error) {
            return std::string(error.what()).find("bad Spark V2 dimensions") !=
                std::string::npos;
        });
}

BOOST_AUTO_TEST_SUITE_END()

}
