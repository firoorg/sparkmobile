#include "../src/chaum.h"
#include "../bitcoin/streams.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

namespace spark {

const int PROTOCOL_VERSION = 90031;

class SparkTest {};

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

    ChaumProof proof;

    Chaum chaum(F, G, H, U);
    chaum.prove(mu, x, y, z, S, T, proof);

    CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
    serialized << proof;

    ChaumProof deserialized;
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

    ChaumProof proof;

    Chaum chaum(F, G, H, U);
    chaum.prove(mu, x, y, z, S, T, proof);

    BOOST_CHECK(chaum.verify(mu, S, T, proof));
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

    ChaumProof proof;

    Chaum chaum(F, G, H, U);
    chaum.prove(mu, x, y, z, S, T, proof);

    // Bad mu
    Scalar evil_mu;
    evil_mu.randomize();
    BOOST_CHECK(!(chaum.verify(evil_mu, S, T, proof)));

    // Bad S
    for (std::size_t i = 0; i < n; i++) {
        std::vector<GroupElement> evil_S(S);
        evil_S[i].randomize();
        BOOST_CHECK(!(chaum.verify(mu, evil_S, T, proof)));
    }

    // Bad T
    for (std::size_t i = 0; i < n; i++) {
        std::vector<GroupElement> evil_T(T);
        evil_T[i].randomize();
        BOOST_CHECK(!(chaum.verify(mu, S, evil_T, proof)));
    }

    // Bad A1
    ChaumProof evil_proof = proof;
    evil_proof.A1.randomize();
    BOOST_CHECK(!(chaum.verify(mu, S, T, evil_proof)));

    // Bad A2
    for (std::size_t i = 0; i < n; i++) {
        evil_proof = proof;
        evil_proof.A2[i].randomize();
        BOOST_CHECK(!(chaum.verify(mu, S, T, evil_proof)));
    }

    // Bad t1
    for (std::size_t i = 0; i < n; i++) {
        evil_proof = proof;
        evil_proof.t1[i].randomize();
        BOOST_CHECK(!(chaum.verify(mu, S, T, evil_proof)));
    }

    // Bad t2
    evil_proof = proof;
    evil_proof.t2.randomize();
    BOOST_CHECK(!(chaum.verify(mu, S, T, evil_proof)));

    // Bad t3
    evil_proof = proof;
    evil_proof.t3.randomize();
    BOOST_CHECK(!(chaum.verify(mu, S, T, evil_proof)));
}

BOOST_AUTO_TEST_SUITE_END()

}
