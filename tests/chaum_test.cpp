#include "../src/chaum.h"
#include "../src/transcript.h"
#include "../bitcoin/streams.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

namespace spark {

const int PROTOCOL_VERSION = 90031;

class SparkTest {};

namespace {

void CheckV2Completeness(const std::size_t n)
{
    GroupElement F, G, H, U;
    F.randomize();
    G.randomize();
    H.randomize();
    U.randomize();

    Scalar mu;
    mu.randomize();
    std::vector<Scalar> x(n), y(n), z(n);
    std::vector<GroupElement> S(n), T(n);
    for (std::size_t i = 0; i < n; ++i) {
        x[i].randomize();
        y[i].randomize();
        z[i].randomize();
        S[i] = F*x[i] + G*y[i] + H*z[i];
        T[i] = (U + G*y[i].negate())*x[i].inverse();
    }

    ChaumV2Context context;
    context.fee = 1234;
    context.transparent_value = 5678;
    context.serialized_outputs = {{0x01, 0x02}, {0x03, 0x04, 0x05}};

    Chaum chaum(F, G, H, U);
    ChaumProofV2 proof;
    chaum.prove_v2(mu, context, x, y, z, S, T, proof);
    BOOST_REQUIRE(chaum.verify_v2(mu, context, S, T, proof));

    CDataStream encoded(SER_NETWORK, PROTOCOL_VERSION);
    encoded << proof;
    ChaumProofV2 decoded;
    encoded >> decoded;
    BOOST_CHECK(chaum.verify_v2(mu, context, S, T, decoded));
    BOOST_CHECK(encoded.empty());
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(spark_chaum_tests, SparkTest)

BOOST_AUTO_TEST_CASE(serialization)
{
    GroupElement F, G, H, U;
    F.randomize();
    G.randomize();
    H.randomize();
    U.randomize();

    const std::size_t n = 3;

    Scalar mu;
    mu.randomize();
    std::vector<Scalar> x, y, z;
    x.resize(n);
    y.resize(n);
    z.resize(n);
    std::vector<GroupElement> S, T;
    S.resize(n);
    T.resize(n);
    for (std::size_t i = 0; i < n; i++) {
        x[i].randomize();
        y[i].randomize();
        z[i].randomize();

        S[i] = F*x[i] + G*y[i] + H*z[i];
        T[i] = (U + G*y[i].negate())*x[i].inverse();
    }

    ChaumProofV1 proof;

    Chaum chaum(F, G, H, U);
    chaum.prove_v1(mu, x, y, z, S, T, proof);

    CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
    serialized << proof;

    ChaumProofV1 deserialized;
    serialized >> deserialized;

    BOOST_CHECK(proof.A1 == deserialized.A1);
    BOOST_CHECK(proof.t2 == deserialized.t2);
    BOOST_CHECK(proof.t3 == deserialized.t3);
    for (std::size_t i = 0; i < n; i++) {
        BOOST_CHECK(proof.A2[i] == deserialized.A2[i]);
        BOOST_CHECK(proof.t1[i] == deserialized.t1[i]);
    }
}

BOOST_AUTO_TEST_CASE(completeness)
{
    GroupElement F, G, H, U;
    F.randomize();
    G.randomize();
    H.randomize();
    U.randomize();

    const std::size_t n = 3;

    Scalar mu;
    mu.randomize();
    std::vector<Scalar> x, y, z;
    x.resize(n);
    y.resize(n);
    z.resize(n);
    std::vector<GroupElement> S, T;
    S.resize(n);
    T.resize(n);
    for (std::size_t i = 0; i < n; i++) {
        x[i].randomize();
        y[i].randomize();
        z[i].randomize();

        S[i] = F*x[i] + G*y[i] + H*z[i];
        T[i] = (U + G*y[i].negate())*x[i].inverse();
    }

    ChaumProofV1 proof;

    Chaum chaum(F, G, H, U);
    chaum.prove_v1(mu, x, y, z, S, T, proof);

    BOOST_CHECK(chaum.verify_v1(mu, S, T, proof));
}

BOOST_AUTO_TEST_CASE(bad_proofs)
{
    GroupElement F, G, H, U;
    F.randomize();
    G.randomize();
    H.randomize();
    U.randomize();

    const std::size_t n = 3;

    Scalar mu;
    mu.randomize();
    std::vector<Scalar> x, y, z;
    x.resize(n);
    y.resize(n);
    z.resize(n);
    std::vector<GroupElement> S, T;
    S.resize(n);
    T.resize(n);
    for (std::size_t i = 0; i < n; i++) {
        x[i].randomize();
        y[i].randomize();
        z[i].randomize();

        S[i] = F*x[i] + G*y[i] + H*z[i];
        T[i] = (U + G*y[i].negate())*x[i].inverse();
    }

    ChaumProofV1 proof;

    Chaum chaum(F, G, H, U);
    chaum.prove_v1(mu, x, y, z, S, T, proof);

    // Bad mu
    Scalar evil_mu;
    evil_mu.randomize();
    BOOST_CHECK(!(chaum.verify_v1(evil_mu, S, T, proof)));

    // Bad S
    for (std::size_t i = 0; i < n; i++) {
        std::vector<GroupElement> evil_S(S);
        evil_S[i].randomize();
        BOOST_CHECK(!(chaum.verify_v1(mu, evil_S, T, proof)));
    }

    // Bad T
    for (std::size_t i = 0; i < n; i++) {
        std::vector<GroupElement> evil_T(T);
        evil_T[i].randomize();
        BOOST_CHECK(!(chaum.verify_v1(mu, S, evil_T, proof)));
    }

    // Bad A1
    ChaumProofV1 evil_proof = proof;
    evil_proof.A1.randomize();
    BOOST_CHECK(!(chaum.verify_v1(mu, S, T, evil_proof)));

    // Bad A2
    for (std::size_t i = 0; i < n; i++) {
        evil_proof = proof;
        evil_proof.A2[i].randomize();
        BOOST_CHECK(!(chaum.verify_v1(mu, S, T, evil_proof)));
    }

    // Bad t1
    for (std::size_t i = 0; i < n; i++) {
        evil_proof = proof;
        evil_proof.t1[i].randomize();
        BOOST_CHECK(!(chaum.verify_v1(mu, S, T, evil_proof)));
    }

    // Bad t2
    evil_proof = proof;
    evil_proof.t2.randomize();
    BOOST_CHECK(!(chaum.verify_v1(mu, S, T, evil_proof)));

    // Bad t3
    evil_proof = proof;
    evil_proof.t3.randomize();
    BOOST_CHECK(!(chaum.verify_v1(mu, S, T, evil_proof)));
}

BOOST_AUTO_TEST_CASE(single_input_verifier_accepts_valid_proof)
{
    GroupElement F, G, H, U;
    F.randomize();
    G.randomize();
    H.randomize();
    U.randomize();

    Scalar mu, x, y, z;
    mu.randomize();
    x.randomize();
    y.randomize();
    z.randomize();
    const std::vector<GroupElement> S{F*x + G*y + H*z};
    const std::vector<GroupElement> T{
        (U + G*y.negate())*x.inverse()};

    Chaum chaum(F, G, H, U);
    ChaumProofV1 proof;
    chaum.prove_v1(mu, {x}, {y}, {z}, S, T, proof);
    BOOST_REQUIRE(chaum.verify_single_input(mu, S, T, proof));

    ChaumProofV1 changed = proof;
    changed.A1.randomize();
    BOOST_CHECK(!chaum.verify_single_input(mu, S, T, changed));
    BOOST_CHECK(!chaum.verify_single_input(mu, {S[0], S[0]}, {T[0], T[0]}, proof));
}

BOOST_AUTO_TEST_CASE(v2_completeness_and_context_binding)
{
    CheckV2Completeness(1);
    CheckV2Completeness(2);
    CheckV2Completeness(MAX_CHAUM_V2_INPUTS);

    GroupElement F, G, H, U;
    F.randomize();
    G.randomize();
    H.randomize();
    U.randomize();

    const std::size_t n = 2;
    Scalar mu;
    mu.randomize();
    std::vector<Scalar> x(n), y(n), z(n);
    std::vector<GroupElement> S(n), T(n);
    for (std::size_t i = 0; i < n; ++i) {
        x[i].randomize();
        y[i].randomize();
        z[i].randomize();
        S[i] = F*x[i] + G*y[i] + H*z[i];
        T[i] = (U + G*y[i].negate())*x[i].inverse();
    }

    ChaumV2Context context;
    context.fee = 11;
    context.transparent_value = 22;
    context.serialized_outputs = {{0xaa}, {0xbb}};
    context.extension_commitment = uint256S("01");
    context.serialized_cover_set_references = {0x01, 0xaa};

    Chaum chaum(F, G, H, U);
    ChaumProofV2 proof;
    chaum.prove_v2(mu, context, x, y, z, S, T, proof);
    BOOST_REQUIRE(chaum.verify_v2(mu, context, S, T, proof));

    ChaumV2Context changed_context = context;
    ++changed_context.fee;
    BOOST_CHECK(!chaum.verify_v2(mu, changed_context, S, T, proof));
    changed_context = context;
    ++changed_context.transparent_value;
    BOOST_CHECK(!chaum.verify_v2(mu, changed_context, S, T, proof));
    changed_context = context;
    changed_context.serialized_outputs[0][0] ^= 1;
    BOOST_CHECK(!chaum.verify_v2(mu, changed_context, S, T, proof));
    changed_context = context;
    changed_context.extension_commitment = uint256S("02");
    BOOST_CHECK(!chaum.verify_v2(mu, changed_context, S, T, proof));
    changed_context = context;
    changed_context.serialized_cover_set_references.back() ^= 1;
    BOOST_CHECK(!chaum.verify_v2(mu, changed_context, S, T, proof));

    Scalar offset;
    do {
        offset.randomize();
    } while (offset.isZero());
    ChaumProofV2 coupled_responses = proof;
    coupled_responses.t2[0] += offset;
    coupled_responses.t2[1] -= offset;
    BOOST_CHECK(!chaum.verify_v2(mu, context, S, T, coupled_responses));
}

BOOST_AUTO_TEST_CASE(v2_rejects_bad_dimensions)
{
    GroupElement F, G, H, U;
    F.randomize();
    G.randomize();
    H.randomize();
    U.randomize();
    Scalar mu;
    mu.randomize();
    Chaum chaum(F, G, H, U);
    ChaumV2Context context;
    ChaumProofV2 proof;

    BOOST_CHECK(!chaum.verify_v2(mu, context, {}, {}, proof));
    BOOST_CHECK_THROW(
        CheckV2Completeness(MAX_CHAUM_V2_INPUTS + 1),
        std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()

}
