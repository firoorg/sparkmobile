#ifndef FIRO_PRIMITIVES_H
#define FIRO_PRIMITIVES_H

#include "coin.h"
#include "../bitcoin/serialize.h"
#include "../bitcoin/uint256.h"
#include <boost/optional.hpp>

struct CSparkMintMeta
{
    int nHeight = -1;
    int nId = -1;
    bool isUsed = false;
    uint256 txid;
    std::uint64_t i = 0; // diversifier
    std::vector<unsigned char> d; // encrypted diversifier
    std::uint64_t v = 0; // value
    Scalar k; // nonce
    std::string memo; // memo
    std::vector<unsigned char> serial_context;
    char type = 0;
    spark::Coin coin;
    mutable boost::optional<uint256> nonceHash;

    uint256 GetNonceHash() const;

    bool operator==(const CSparkMintMeta& other) const
    {
        return this->k == other.k;
    }

    bool operator!=(const CSparkMintMeta& other) const
    {
        return this->k != other.k;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nHeight);
        READWRITE(nId);
        READWRITE(isUsed);
        READWRITE(txid);
        READWRITE(i);
        READWRITE(d);
        READWRITE(v);
        READWRITE(k);
        READWRITE(memo);
        READWRITE(serial_context);
        READWRITE(type);
        READWRITE(coin);
    };
};

class CSparkSpendEntry
{
public:
    GroupElement lTag;
    uint256 lTagHash;
    uint256 hashTx;
    int64_t amount;

    CSparkSpendEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        lTag = GroupElement();
        lTagHash = uint256();
        amount = 0;
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(lTag);
        READWRITE(lTagHash);
        READWRITE(hashTx);
        READWRITE(amount);
    }
};

namespace primitives {
    uint256 GetNonceHash(const secp_primitives::Scalar& nonce);
    uint256 GetLTagHash(const secp_primitives::GroupElement& tag);
    uint256 GetSparkCoinHash(const spark::Coin& coin);
}

namespace spark {
// Custom hash for the spark coin. norte. THIS IS NOT SECURE HASH FUNCTION
struct CoinHash {
    std::size_t operator()(const spark::Coin& coin) const noexcept;
};

// Custom hash for the linking tag. THIS IS NOT SECURE HASH FUNCTION
struct CLTagHash {
    std::size_t operator()(const secp_primitives::GroupElement& tag) const noexcept;
};

struct CMintedCoinInfo {
    int coinGroupId;
    int nHeight;

    static CMintedCoinInfo make(int coinGroupId, int nHeight);
};

}


#endif //FIRO_PRIMITIVES_H
