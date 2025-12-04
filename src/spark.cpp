#include "../include/spark.h"
#include "spark.h"

//#define __EMSCRIPTEN__ 1   // useful to uncomment - just for development (when building, needs to be defined project-wide, not just in this file)
#ifdef __EMSCRIPTEN__
#include <iostream>
#include <iomanip>
#include <sstream>
#include <boost/scope_exit.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include "../bitcoin/utilstrencodings.h"
#endif

//#define SPARK_DEBUGGING_OUTPUT 1 // useful to uncomment - for building a testing module with outputs to help with troubleshooting. Make sure it is commented out in prod builds!

//#include "../bitcoin/amount.h"
//#include <iostream>

#define SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION     (10000 * COIN)


spark::SpendKey createSpendKey(const SpendKeyData& data) {
    std::string nCountStr = std::to_string(data.getIndex());
    CHash256 hasher;
    std::string prefix = "r_generation";
    hasher.Write(reinterpret_cast<const unsigned char*>(prefix.c_str()), prefix.size());
    hasher.Write(data.getKeyData(), data.size());
    hasher.Write(reinterpret_cast<const unsigned char*>(nCountStr.c_str()), nCountStr.size());
    unsigned char hash[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(hash);

    secp_primitives::Scalar r;
    r.memberFromSeed(hash);
    spark::SpendKey key(spark::Params::get_default(), r);
    return key;
}

spark::FullViewKey createFullViewKey(const spark::SpendKey& spendKey) {
    return spark::FullViewKey(spendKey);
}

spark::IncomingViewKey createIncomingViewKey(const spark::FullViewKey& fullViewKey) {
    return spark::IncomingViewKey(fullViewKey);
}

template <typename Iterator>
static uint64_t CalculateSparkCoinsBalance(Iterator begin, Iterator end)
{
    uint64_t balance = 0;
    for (auto start = begin; start != end; ++start) {
        balance += start->v;
    }
    return balance;
}

std::vector<CRecipient> createSparkMintRecipients(
                        const std::vector<spark::MintedCoinData>& outputs,
                        const std::vector<unsigned char>& serial_context,
                        bool generate)
{
    const spark::Params* params = spark::Params::get_default();

    spark::MintTransaction sparkMint(params, outputs, serial_context, generate);

    if (generate && !sparkMint.verify()) {
        throw std::runtime_error("Unable to validate spark mint.");
    }

    std::vector<CDataStream> serializedCoins = sparkMint.getMintedCoinsSerialized();

    if (outputs.size() != serializedCoins.size())
        throw std::runtime_error("Spark mint output number should be equal to required number.");

    std::vector<CRecipient> results;
    results.reserve(outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        CScript script;
        script << OP_SPARKMINT;
        script.insert(script.end(), serializedCoins[i].begin(), serializedCoins[i].end());
        CRecipient recipient = {script, CAmount(outputs[i].v), false};
        results.emplace_back(recipient);
    }
    return results;
}

bool GetCoinsToSpend(
        CAmount required,
        std::vector<CSparkMintMeta>& coinsToSpend_out,
        std::list<CSparkMintMeta> coins,
        int64_t& changeToMint)
{
    CAmount availableBalance = CalculateSparkCoinsBalance(coins.begin(), coins.end());

    if (required > availableBalance) {
        throw std::runtime_error("Insufficient funds !");
    }

    // sort by biggest amount. if it is same amount we will prefer the older block
    auto comparer = [](const CSparkMintMeta& a, const CSparkMintMeta& b) -> bool {
        return a.v != b.v ? a.v > b.v : a.nHeight < b.nHeight;
    };
    coins.sort(comparer);

    CAmount spend_val(0);

    std::list<CSparkMintMeta> coinsToSpend;
    while (spend_val < required) {
        if (coins.empty())
            break;

        CSparkMintMeta choosen;
        CAmount need = required - spend_val;

        auto itr = coins.begin();
        if (need >= itr->v) {
            choosen = *itr;
            coins.erase(itr);
        } else {
            for (auto coinIt = coins.rbegin(); coinIt != coins.rend(); coinIt++) {
                auto nextItr = coinIt;
                nextItr++;

                if (coinIt->v >= need && (nextItr == coins.rend() || nextItr->v != coinIt->v)) {
                    choosen = *coinIt;
                    coins.erase(std::next(coinIt).base());
                    break;
                }
            }
        }

        spend_val += choosen.v;
        coinsToSpend.push_back(choosen);
    }

    // sort by group id ay ascending order. it is mandatory for creting proper joinsplit
    auto idComparer = [](const CSparkMintMeta& a, const CSparkMintMeta& b) -> bool {
        return a.nId < b.nId;
    };
    coinsToSpend.sort(idComparer);

    changeToMint = spend_val - required;
    coinsToSpend_out.insert(coinsToSpend_out.begin(), coinsToSpend.begin(), coinsToSpend.end());

    return true;
}

std::pair<CAmount, std::vector<CSparkMintMeta>> SelectSparkCoins(
        CAmount required,
        bool subtractFeeFromAmount,
        const std::list<CSparkMintMeta>& coins,
        std::size_t mintNum,
        std::size_t utxoNum,
        std::size_t additionalTxSize) {
    CFeeRate fRate;

    CAmount fee;
    unsigned size;
    int64_t changeToMint = 0; // this value can be negative, that means we need to spend remaining part of required value with another transaction (nMaxInputPerTransaction exceeded)

    std::vector<CSparkMintMeta> spendCoins;
    for (fee = fRate.GetFeePerK();;) {
        CAmount currentRequired = required;

        if (!subtractFeeFromAmount)
            currentRequired += fee;
        spendCoins.clear();
        if (!GetCoinsToSpend(currentRequired, spendCoins, coins, changeToMint)) {
            throw std::invalid_argument("Unable to select coins for spend");
        }

        // 1803 is for first grootle proof/aux data
        // 213 for each private output, 34 for each utxo,924 constant parts of tx parts of tx,
        size = 924 + 1803*(spendCoins.size()) + 322*(mintNum+1) + 34 * utxoNum + additionalTxSize;
        CAmount feeNeeded = size;

        if (fee >= feeNeeded) {
            break;
        }

        fee = feeNeeded;

        if (subtractFeeFromAmount)
            break;
    }

    if (changeToMint < 0)
        throw std::invalid_argument("Unable to select coins for spend");

    return std::make_pair(fee, spendCoins);

}


bool getIndex(const spark::Coin& coin, const std::vector<spark::Coin>& anonymity_set, size_t& index) {
    for (std::size_t j = 0; j < anonymity_set.size(); ++j) {
        if (anonymity_set[j] == coin){
            index = j;
            return true;
        }
    }
    return false;
}

#define AUTOCOMPUTE_SPARK_SPEND_TX_HASH 1
#if AUTOCOMPUTE_SPARK_SPEND_TX_HASH
struct CMutableTransaction;

class COutPoint
{
public:
   uint256 hash;
   uint32_t n;

   COutPoint() { SetNull(); }
   COutPoint(uint256 hashIn, uint32_t nIn) { hash = hashIn; n = nIn; }

   ADD_SERIALIZE_METHODS;

   template <typename Stream, typename Operation>
   inline void SerializationOp(Stream& s, Operation ser_action) {
      READWRITE(hash);
      READWRITE(n);
   }

   void SetNull() { hash.SetNull(); n = (uint32_t) -1; }
   bool IsNull() const { return (hash.IsNull() && n == (uint32_t) -1); }

   bool IsSigmaMintGroup() const { return hash.IsNull() && n >= 1; }

   friend bool operator<(const COutPoint& a, const COutPoint& b)
   {
      int cmp = a.hash.Compare(b.hash);
      return cmp < 0 || (cmp == 0 && a.n < b.n);
   }

   friend bool operator==(const COutPoint& a, const COutPoint& b)
   {
      return (a.hash == b.hash && a.n == b.n);
   }

   friend bool operator!=(const COutPoint& a, const COutPoint& b)
   {
      return !(a == b);
   }

   std::string ToString() const;
   std::string ToStringShort() const;
};

class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;
    CScript prevPubKey;
    CScriptWitness scriptWitness; //! Only serialized through CTransaction

    /* Setting nSequence to this value for every input in a transaction
     * disables nLockTime. */
    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    /* Below flags apply in the context of BIP 68*/
    /* If this flag set, CTxIn::nSequence is NOT interpreted as a
     * relative lock-time. */
    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1 << 31);

    /* If CTxIn::nSequence encodes a relative lock-time and this flag
     * is set, the relative lock-time has units of 512 seconds,
     * otherwise it specifies blocks with a granularity of 1. */
    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22);

    /* If CTxIn::nSequence encodes a relative lock-time, this mask is
     * applied to extract that lock-time from the sequence field. */
    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

    /* In order to use the same number of bits to encode roughly the
     * same wall-clock duration, and because blocks are naturally
     * limited to occur every 600s on average, the minimum granularity
     * for time-based relative lock-time is fixed at 512 seconds.
     * Converting from CTxIn::nSequence to seconds is performed by
     * multiplying by 512 = 2^9, or equivalently shifting up by
     * 9 bits. */
    static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;

    CTxIn()
    {
        nSequence = SEQUENCE_FINAL;
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(prevout);
        READWRITE(*(CScriptBase*)(&scriptSig));
        READWRITE(nSequence);
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    friend bool operator<(const CTxIn& a, const CTxIn& b)
    {
        return a.prevout<b.prevout;
    }

    std::string ToString() const;
    bool IsZerocoinSpend() const;
    bool IsSigmaSpend() const;
    bool IsLelantusJoinSplit() const;
    bool IsZerocoinRemint() const;
};

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
   prevout = prevoutIn;
   scriptSig = scriptSigIn;
   nSequence = nSequenceIn;
}

class CTxOut
{
public:
    CAmount nValue;
    CScript scriptPubKey;
    int nRounds;

    CTxOut()
    {
        SetNull();
    }

    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nValue);
        READWRITE(*(CScriptBase*)(&scriptPubKey));
        if (ser_action.ForRead())
            nRounds = -10;
    }

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
        nRounds = -10; // an initial value, should be no way to get this by calculations
    }

    bool IsNull() const
    {
        return (nValue == -1);
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    friend bool operator<(const CTxOut& a, const CTxOut& b)
    {
        return a.nValue < b.nValue || (a.nValue == b.nValue && a.scriptPubKey < b.scriptPubKey);
    }

    uint256 GetHash() const { return SerializeHash(*this); }

    std::string ToString() const;
};

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn)
{
   nValue = nValueIn;
   scriptPubKey = std::move(scriptPubKeyIn);
}

template<typename Stream, typename TxType>
inline void SerializeTransaction(const TxType& tx, Stream& s) {
   const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

   int32_t n32bitVersion = tx.nVersion | (tx.nType << 16);
   s << n32bitVersion;
   unsigned char flags = 0;
   // Consistency check
   if (fAllowWitness) {
      /* Check whether witnesses need to be serialized. */
      if (tx.HasWitness()) {
         flags |= 1;
      }
   }
   if (flags) {
      /* Use extended format in case witnesses are to be serialized. */
      std::vector<CTxIn> vinDummy;
      s << vinDummy;
      s << flags;
   }
   s << tx.vin;
   s << tx.vout;
   if (flags & 1) {
      for (size_t i = 0; i < tx.vin.size(); i++) {
         s << tx.vin[i].scriptWitness.stack;
      }
   }
   s << tx.nLockTime;
   if (tx.nVersion == 3 && tx.nType != TRANSACTION_NORMAL)
      s << tx.vExtraPayload;
}

class CTransaction
{
public:
    // Default transaction version.
    static const int32_t CURRENT_VERSION=1;

    // Changing the default transaction version requires a two step process: first
    // adapting relay policy by bumping MAX_STANDARD_VERSION, and then later date
    // bumping the default CURRENT_VERSION at which point both CURRENT_VERSION and
    // MAX_STANDARD_VERSION will be equal.
    static const int32_t MAX_STANDARD_VERSION=3;

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const int32_t nVersion;
    const int16_t nType;
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const uint32_t nLockTime;
    const std::vector<uint8_t> vExtraPayload; // only available for special transaction types

private:
    /** Memory only. */
    const uint256 hash;

    uint256 ComputeHash() const;

public:
    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction();

    /** Convert a CMutableTransaction into a CTransaction. */
    CTransaction(const CMutableTransaction &tx);
    CTransaction(CMutableTransaction &&tx);

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeTransaction(*this, s);
    }

    /** This deserializing constructor is provided instead of an Unserialize method.
     *  Unserialize is not possible, since it would require overwriting const fields. */
    template <typename Stream>
    CTransaction(deserialize_type, Stream& s) : CTransaction(CMutableTransaction(deserialize, s)) {}

    bool IsNull() const {
        return vin.empty() && vout.empty();
    }

    const uint256& GetHash() const {
        return hash;
    }

    // Compute a hash that includes both transaction and witness data
    uint256 GetWitnessHash() const;

    // Return sum of txouts.
    CAmount GetValueOut() const;
    // GetValueIn() is a method on CCoinsViewCache, because
    // inputs must be known to compute value in.

    // Compute priority, given priority of inputs and (optionally) tx size
    double ComputePriority(double dPriorityInputs, unsigned int nTxSize=0) const;

    // Compute modified tx size for priority calculation (optionally given tx size)
    unsigned int CalculateModifiedSize(unsigned int nTxSize=0) const;

    // Returns true, if this is any zerocoin transaction.
    bool IsZerocoinTransaction() const;

    // Returns true, if this is a V3 zerocoin mint or spend, made with sigma algorithm.
    bool IsZerocoinV3SigmaTransaction() const;

    bool IsLelantusTransaction() const;

    bool IsZerocoinSpend() const;
    bool IsZerocoinMint() const;

    bool IsSigmaSpend() const;
    bool IsSigmaMint() const;

    bool IsLelantusJoinSplit() const;
    bool IsLelantusMint() const;

    bool IsZerocoinRemint() const;

    bool IsSparkTransaction() const;
    bool IsSparkSpend() const;
    bool IsSparkMint() const;

    bool IsSpatsTransaction() const;
    bool IsSpatsCreate() const;
    bool IsSpatsUnregister() const;
    bool IsSpatsModify() const;
    bool IsSpatsMint() const;
    bool IsSpatsBurn() const;

    bool HasNoRegularInputs() const;
    bool HasPrivateInputs() const;

    /**
     * Get the total transaction size in bytes, including witness data.
     * "Total Size" defined in BIP141 and BIP144.
     * @return Total transaction size in bytes
     */
    unsigned int GetTotalSize() const;

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return a.hash != b.hash;
    }

    std::string ToString() const;

    bool HasWitness() const
    {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }
};

struct CMutableTransaction
{
   int32_t nVersion;
   int32_t nType;
   std::vector<CTxIn> vin;
   std::vector<CTxOut> vout;
   uint32_t nLockTime;
   std::vector<uint8_t> vExtraPayload; // only available for special transaction types

   CMutableTransaction();
   CMutableTransaction(const CTransaction& tx);

   template <typename Stream>
   inline void Serialize(Stream& s) const {
      SerializeTransaction(*this, s);
   }


   template <typename Stream>
   inline void Unserialize(Stream& s) {
      UnserializeTransaction(*this, s);
   }

   template <typename Stream>
   CMutableTransaction(deserialize_type, Stream& s) {
      Unserialize(s);
   }

   /** Compute the hash of this CMutableTransaction. This is computed on the
    * fly, as opposed to GetHash() in CTransaction, which uses a cached result.
    */
   uint256 GetHash() const;

   std::string ToString() const;
   friend bool operator==(const CMutableTransaction& a, const CMutableTransaction& b)
   {
      return a.GetHash() == b.GetHash();
   }

   friend bool operator!=(const CMutableTransaction& a, const CMutableTransaction& b)
   {
      return !(a == b);
   }

   bool HasWitness() const
   {
      for (size_t i = 0; i < vin.size(); i++) {
         if (!vin[i].scriptWitness.IsNull()) {
            return true;
         }
      }
      return false;
   }
};

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nType(TRANSACTION_NORMAL), nLockTime(0) {}

uint256 CMutableTransaction::GetHash() const
{
   return SerializeHash(*this, SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS);
}

uint256 computeSparkSpendTransactionHash( const std::vector<CRecipient>& recipients )
{
    CMutableTransaction tx;
    tx.nLockTime = 1000;
    for (size_t i = 0; i < recipients.size(); i++) {
       auto& recipient = recipients[i];
       if (recipient.amount == 0)
          continue;

       CTxOut vout(recipient.amount, recipient.pubKey);
       tx.vout.push_back(vout);
    }

    uint32_t sequence = CTxIn::SEQUENCE_FINAL;
    CScript script;
    script << OP_SPARKSPEND;
    tx.vin.emplace_back(COutPoint(), script, sequence);

    tx.vExtraPayload.clear();
    // set correct type of transaction (this affects metadata hash)
    tx.nVersion = 3;
    tx.nType = TRANSACTION_SPARK;
    return tx.GetHash();
}
#endif

void createSparkSpendTransaction(
        const spark::SpendKey& spendKey,
        const spark::FullViewKey& fullViewKey,
        const spark::IncomingViewKey& incomingViewKey,
        const std::vector<std::pair<CAmount, bool>>& recipients,
        const std::vector<std::pair<spark::OutputCoinData, bool>>& privateRecipients,
        const std::list<CSparkMintMeta>& coins,
        const std::unordered_map<uint64_t, spark::CoverSetData>& cover_set_data_all,
        const std::map<uint64_t, uint256>& idAndBlockHashes_all,
        const uint256& txHashSig,
        std::size_t additionalTxSize,
        CAmount &fee,
        std::vector<uint8_t>& serializedSpend,
        std::vector<std::vector<unsigned char>>& outputScripts,
        std::vector<CSparkMintMeta>& spentCoinsOut) {

    if (recipients.empty() && privateRecipients.empty()) {
        throw std::runtime_error("Either recipients or privateRecipients has to be nonempty.");
    }

    if (privateRecipients.size() >= SPARK_OUT_LIMIT_PER_TX - 1)
        throw std::runtime_error("Spark shielded output limit exceeded.");

    // calculate total value to spend
    CAmount vOut = 0;
    CAmount mintVOut = 0;
    unsigned recipientsToSubtractFee = 0;

    for (size_t i = 0; i < recipients.size(); i++) {
        auto& recipient = recipients[i];

        if (!MoneyRange(recipient.first)) {
            throw std::runtime_error("Recipient has invalid amount");
        }

        vOut += recipient.first;

        if (recipient.second) {
            recipientsToSubtractFee++;
        }
    }

    for (const auto& privRecipient : privateRecipients) {
        mintVOut += privRecipient.first.v;
        if (privRecipient.second) {
            recipientsToSubtractFee++;
        }
    }

    if (vOut > SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION)
        throw std::runtime_error("Spend to transparent address limit exceeded (10,000 Firo per transaction).");

    std::pair<CAmount, std::vector<CSparkMintMeta>> estimated =
            SelectSparkCoins(vOut + mintVOut, recipientsToSubtractFee, coins, privateRecipients.size(), recipients.size(), additionalTxSize);

    auto recipients_ = recipients;
    auto privateRecipients_ = privateRecipients;
    {
        bool remainderSubtracted = false;
        fee = estimated.first;
        for (size_t i = 0; i < recipients_.size(); i++) {
            auto &recipient = recipients_[i];

            if (recipient.second) {
                // Subtract fee equally from each selected recipient.
                recipient.first -= fee / recipientsToSubtractFee;

                if (!remainderSubtracted) {
                    // First receiver pays the remainder not divisible by output count.
                    recipient.first -= fee % recipientsToSubtractFee;
                    remainderSubtracted = true;
                }
            }
        }

        for (size_t i = 0; i < privateRecipients_.size(); i++) {
            auto &privateRecipient = privateRecipients_[i];

            if (privateRecipient.second) {
                // Subtract fee equally from each selected recipient.
                privateRecipient.first.v -= fee / recipientsToSubtractFee;

                if (!remainderSubtracted) {
                    // First receiver pays the remainder not divisible by output count.
                    privateRecipient.first.v -= fee % recipientsToSubtractFee;
                    remainderSubtracted = true;
                }
            }
        }

    }

    const spark::Params* params = spark::Params::get_default();
    if (spendKey == spark::SpendKey(params))
        throw std::runtime_error("Invalid spend key.");

    CAmount spendInCurrentTx = 0;
    for (auto& spendCoin : estimated.second)
        spendInCurrentTx += spendCoin.v;
    spendInCurrentTx -= fee;

    uint64_t transparentOut = 0;
    // fill outputs
    for (size_t i = 0; i < recipients_.size(); i++) {
        auto& recipient = recipients_[i];
        if (recipient.first == 0)
            continue;

        transparentOut += recipient.first;
    }

    spendInCurrentTx -= transparentOut;
    std::vector<spark::OutputCoinData> privOutputs;
    // fill outputs
    for (size_t i = 0; i < privateRecipients_.size(); i++) {
        auto& recipient = privateRecipients_[i];
        if (recipient.first.v == 0)
            continue;

        CAmount recipientAmount = recipient.first.v;
        spendInCurrentTx -= recipientAmount;
        spark::OutputCoinData output = recipient.first;
        output.v = recipientAmount;
        privOutputs.push_back(output);
    }

    if (spendInCurrentTx < 0)
        throw std::invalid_argument("Unable to create spend transaction.");

    if (!privOutputs.size() || spendInCurrentTx > 0) {
        spark::OutputCoinData output;
        output.address = spark::Address(incomingViewKey, SPARK_CHANGE_D);
        output.memo = "";
        if (spendInCurrentTx > 0)
            output.v = spendInCurrentTx;
        else
            output.v = 0;
        privOutputs.push_back(output);
    }

    // clear vExtraPayload to calculate metadata hash correctly
    serializedSpend.clear();

    // We will write this into cover set representation, with anonymity set hash
    uint256 sig = txHashSig;
#if AUTOCOMPUTE_SPARK_SPEND_TX_HASH
    if (sig.IsNull()) {
       std::vector<CRecipient> recipients;
       for (const auto& r : recipients_) {
           CRecipient recipient;
           recipient.amount = r.first;
           recipient.subtractFeeFromAmount = r.second;
           // TODO recipient.scriptPubKey = GetScriptForDestination(r.address);
           recipients.push_back(recipient);
       }
       // TODO turned off for now sig = computeSparkSpendTransactionHash(recipients);
    }
    std::cout << "sig: " << sig.ToString() << std::endl;
#endif

    std::vector<spark::InputCoinData> inputs;
    std::map<uint64_t, uint256> idAndBlockHashes;
    std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data;
    for (auto& coin : estimated.second) {
        uint64_t groupId = coin.nId;
        if (cover_set_data.count(groupId) == 0) {
            if (!(cover_set_data_all.count(groupId) > 0 && idAndBlockHashes_all.count(groupId) > 0 ))
                throw std::runtime_error("No such coin in set in input data: " + std::to_string(groupId));
            cover_set_data[groupId] = cover_set_data_all.at(groupId);
            idAndBlockHashes[groupId] = idAndBlockHashes_all.at(groupId);
            cover_set_data[groupId].cover_set_representation.insert(cover_set_data[groupId].cover_set_representation.end(), sig.begin(), sig.end());

        }

        spark::InputCoinData inputCoinData;
        inputCoinData.cover_set_id = groupId;
        std::size_t index = 0;
        if (!getIndex(coin.coin, cover_set_data[groupId].cover_set, index))
            throw std::runtime_error("No such coin in set");
        inputCoinData.index = index;
        inputCoinData.v = coin.v;
        inputCoinData.k = coin.k;

        spark::IdentifiedCoinData identifiedCoinData;
        identifiedCoinData.i = coin.i;
        identifiedCoinData.d = coin.d;
        identifiedCoinData.v = coin.v;
        identifiedCoinData.k = coin.k;
        identifiedCoinData.memo = coin.memo;
        spark::RecoveredCoinData recoveredCoinData = coin.coin.recover(fullViewKey, identifiedCoinData);

        inputCoinData.T = recoveredCoinData.T;
        inputCoinData.s = recoveredCoinData.s;
        inputs.push_back(inputCoinData);
    }

    spark::SpendTransaction spendTransaction(params, fullViewKey, spendKey, inputs, cover_set_data, fee, transparentOut, privOutputs);
    spendTransaction.setBlockHashes(idAndBlockHashes);
    CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
    serialized << spendTransaction;
    serializedSpend.assign(serialized.begin(), serialized.end());

    outputScripts.clear();
    const std::vector<spark::Coin>& outCoins = spendTransaction.getOutCoins();
    for (auto& outCoin : outCoins) {
        // construct spend script
        CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
        serialized << outCoin;
        std::vector<unsigned char> script;
        script.push_back((unsigned char)OP_SPARKSMINT);
        script.insert(script.end(), serialized.begin(), serialized.end());
        outputScripts.emplace_back(script);
    }

        spentCoinsOut = estimated.second;
}

spark::Address getAddress(const spark::IncomingViewKey& incomingViewKey, const std::uint64_t diversifier)
{
    return spark::Address(incomingViewKey, diversifier);
}

bool isValidSparkAddress( const char* address_cstr, bool is_test_net ) noexcept
{
    if (!address_cstr || !*address_cstr)
       return false;
    try {
        spark::Address address;
        const unsigned char network = address.decode(address_cstr);
        return network == (is_test_net ? spark::ADDRESS_NETWORK_TESTNET : spark::ADDRESS_NETWORK_MAINNET);
    } catch (...) {
        return false;
    }
}

spark::Coin getCoinFromMeta(const CSparkMintMeta& meta, const spark::IncomingViewKey& incomingViewKey)
{
    const auto* params = spark::Params::get_default();
    spark::Address address(incomingViewKey, meta.i);
    return spark::Coin(params, meta.type, meta.k, address, meta.v, meta.memo, meta.serial_context);
}

spark::IdentifiedCoinData identifyCoin(spark::Coin coin, const spark::IncomingViewKey& incoming_view_key)
{
    return coin.identify(incoming_view_key);
}

void getSparkSpendScripts(const spark::FullViewKey& fullViewKey,
                          const spark::SpendKey& spendKey,
                          const std::vector<spark::InputCoinData>& inputs,
                          const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data,
                          const std::map<uint64_t, uint256>& idAndBlockHashes,
                          CAmount fee,
                          uint64_t transparentOut,
                          const std::vector<spark::OutputCoinData>& privOutputs,
                          std::vector<uint8_t>& inputScript, std::vector<std::vector<unsigned char>>& outputScripts)
{
    inputScript.clear();
    outputScripts.clear();
    const auto* params = spark::Params::get_default();
    spark::SpendTransaction spendTransaction(params, fullViewKey, spendKey, inputs, cover_set_data, fee, transparentOut, privOutputs);
    spendTransaction.setBlockHashes(idAndBlockHashes);
    CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
    serialized << spendTransaction;

    inputScript = std::vector<uint8_t>(serialized.begin(), serialized.end());

    const std::vector<spark::Coin>& outCoins = spendTransaction.getOutCoins();
    for (auto& outCoin : outCoins) {
        // construct spend script
        CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
        serialized << outCoin;
        std::vector<unsigned char> script;
        script.insert(script.end(), serialized.begin(), serialized.end());
        outputScripts.emplace_back(script);
    }
}

void ParseSparkMintTransaction(const std::vector<CScript>& scripts, spark::MintTransaction& mintTransaction)
{
    std::vector<CDataStream> serializedCoins;
    for (const auto& script : scripts) {
        if (!script.IsSparkMint())
            throw std::invalid_argument("Script is not a Spark mint");

        std::vector<unsigned char> serialized(script.begin() + 1, script.end());
        size_t size = spark::Coin::memoryRequired() + 8; // 8 is the size of uint64_t
        if (serialized.size() < size) {
            throw std::invalid_argument("Script is not a valid Spark mint");
        }

        CDataStream stream(
                std::vector<unsigned char>(serialized.begin(), serialized.end()),
                SER_NETWORK,
                PROTOCOL_VERSION
        );

        serializedCoins.push_back(stream);
    }
    try {
        mintTransaction.setMintTransaction(serializedCoins);
    } catch (...) {
        throw std::invalid_argument("Unable to deserialize Spark mint transaction");
    }
}

void ParseSparkMintCoin(const CScript& script, spark::Coin& txCoin)
{
    if (!script.IsSparkMint() && !script.IsSparkSMint())
        throw std::invalid_argument("Script is not a Spark mint");

    if (script.size() < 213) {
        throw std::invalid_argument("Script is not a valid Spark Mint");
    }

    std::vector<unsigned char> serialized(script.begin() + 1, script.end());
    CDataStream stream(
            std::vector<unsigned char>(serialized.begin(), serialized.end()),
            SER_NETWORK,
            PROTOCOL_VERSION
    );

    try {
        stream >> txCoin;
    } catch (...) {
        throw std::invalid_argument("Unable to deserialize Spark mint");
    }
}

CSparkMintMeta getMetadata(const spark::Coin& coin, const spark::IncomingViewKey& incoming_view_key) {
    CSparkMintMeta meta;
    spark::IdentifiedCoinData identifiedCoinData;
    try {
        identifiedCoinData = identifyCoin(coin, incoming_view_key);
    } catch (...) {
        assert(meta.k.isZero());
        return meta;
    }

    meta.isUsed = false;
    meta.v = identifiedCoinData.v;
    meta.memo = identifiedCoinData.memo;
    meta.d = identifiedCoinData.d;
    meta.i = identifiedCoinData.i;
    meta.k = identifiedCoinData.k;
    meta.serial_context = {};
    meta.coin = coin;

    assert(!meta.k.isZero());
    return meta;
}

spark::InputCoinData getInputData(spark::Coin coin, const spark::FullViewKey& full_view_key, const spark::IncomingViewKey& incoming_view_key)
{
    spark::InputCoinData inputCoinData;
    spark::IdentifiedCoinData identifiedCoinData;
    try {
        identifiedCoinData = identifyCoin(coin, incoming_view_key);
    } catch (...) {
        assert(inputCoinData.k.isZero());
        return inputCoinData;
    }

    spark::RecoveredCoinData recoveredCoinData = coin.recover(full_view_key, identifiedCoinData);
    inputCoinData.T = recoveredCoinData.T;
    inputCoinData.s = recoveredCoinData.s;
    inputCoinData.k = identifiedCoinData.k;
    inputCoinData.v = identifiedCoinData.v;

    assert(!inputCoinData.k.isZero());
    return inputCoinData;
}

spark::InputCoinData getInputData(std::pair<spark::Coin, CSparkMintMeta> coin, const spark::FullViewKey& full_view_key)
{
    spark::IdentifiedCoinData identifiedCoinData;
    identifiedCoinData.k = coin.second.k;
    identifiedCoinData.v = coin.second.v;
    identifiedCoinData.d = coin.second.d;
    identifiedCoinData.memo = coin.second.memo;
    identifiedCoinData.i = coin.second.i;

    spark::RecoveredCoinData recoveredCoinData = coin.first.recover(full_view_key, identifiedCoinData);
    spark::InputCoinData inputCoinData;
    inputCoinData.T = recoveredCoinData.T;
    inputCoinData.s = recoveredCoinData.s;
    inputCoinData.k = identifiedCoinData.k;
    inputCoinData.v = identifiedCoinData.v;

    return inputCoinData;
}

spark::Coin deserializeCoin( const unsigned char * const serialized_coin, const std::uint32_t serialized_data_length,
                             const unsigned char * const serial_context, const std::uint32_t serial_context_length )
{
    const std::vector< unsigned char > vec( serialized_coin, serialized_coin + serialized_data_length );
    CDataStream stream( vec, SER_NETWORK, PROTOCOL_VERSION );
    spark::Coin coin( spark::Params::get_default() );
    stream >> coin;
    coin.setSerialContext( { serial_context, serial_context + serial_context_length } );
    return coin;
}

#if 0 // could be useful for some advanced troubleshooting
static spark::IncomingViewKey deserializeIncomingViewKey( const unsigned char * const p, const std::size_t len )
{
    const std::vector< unsigned char > vec( p, p + len );
    CDataStream stream( vec, SER_NETWORK, PROTOCOL_VERSION );
    spark::IncomingViewKey incoming_view_key;
    stream >> incoming_view_key;
    return incoming_view_key;
}
#endif

#ifdef __EMSCRIPTEN__

static std::string to_hex_string( const unsigned char * const data, const size_t len )
{
   std::ostringstream ss;
   ss << std::hex << std::setfill( '0' );
   for ( std::size_t i = 0; i < len; ++i )
      ss << std::setw( 2 ) << +data[ i ];
   return ss.str();
}

#if SPARK_DEBUGGING_OUTPUT

template < class C >
struct container_streamer {
   explicit container_streamer( const C &c ) : c_( c ) {}

   friend std::ostream &operator<<( std::ostream &os, const container_streamer &s )
   {
      // if size > 40, printing just first & last 20, otherwise all of the elements
      const auto count = s.c_.size();
      if ( count == 0 )
         return os << "[]";
      const bool too_large = count > 40;
      os << '[' << count << "| ";
      bool first = true;
#if __cplusplus >= 202002L  // TODO
      for ( const auto &e : s.c_ | std::views::take( too_large ? 20 : count ) ) {
#else
      auto n = too_large ? 20 : count;
      for ( auto it = s.c_.cbegin(); n; --n, ++it ) {
         const auto &e = *it;
#endif
         if ( !first ) {
            os << ", ";
            first = false;
         }
         os << e;
      }
      if ( too_large ) {
         os << ", ...";
#if __cplusplus >= 202002L  // TODO
         for ( const auto &e : s.c_ | std::views::drop( count - 20 ) ) {
#else
         for ( auto it = std::prev( s.c_.cend(), 20 ); it != s.c_.end(); ++it ) {
            const auto &e = *it;
#endif
            os << ", " << e;
         }
      }
      return os << " ]";
   }

private:
   const C &c_;
};

namespace std {
template < typename  F, typename S >
std::ostream &operator<<( std::ostream &os, const std::pair< F, S > &p )
{
   return os << p.first << " -> " << p.second;
}
}

template < unsigned int Bits >
std::ostream &operator<<( std::ostream &os, const base_blob< Bits > &b )
{
   return os << b.GetHex();
}

namespace spark {

std::ostream &operator<<( std::ostream &os, const Coin &c )
{
   return os << "{ hash=" << c.getHash() << " }";
}

std::ostream &operator<<( std::ostream &os, const CoverSetData &c )
{
   return os << container_streamer( c.cover_set ) << " " << to_hex_string( c.cover_set_representation.data(), c.cover_set_representation.size() );
}

std::ostream &operator<<( std::ostream &os, const OutputCoinData &d )
{
   return os << "{ address=" << d.address.encode( ADDRESS_NETWORK_MAINNET ) << ", v=" << d.v << ", memo='" << d.memo << "' }";
}

}

std::ostream &operator<<( std::ostream &os, const CSparkMintMeta &m )
{
   return os << "{ height=" << m.nHeight << ", id=" << m.nId << ", used=" << m.isUsed << ", txid = " << m.txid << ", v=" << m.v << ", i=" << m.i << ", k=" << m.k << ", memo='" << m.memo
             << "', type=" << +m.type << ", d=" << to_hex_string( m.d.data(), m.d.size() ) << ", coin=" << m.coin << " }";
}

#endif   // SPARK_DEBUGGING_OUTPUT

extern "C" {

SpendKeyData *js_createSpendKeyData( const unsigned char * const keydata, const std::int32_t index )
{
    try {
        if ( !keydata ) {
           std::cerr << "Error calling createSpendKeyData: Provided keydata is null." << std::endl;
           return nullptr;
        }
#if SPARK_DEBUGGING_OUTPUT
        std::cout << "createSpendKeyData: " << to_hex_string( keydata, 32 ) << ' ' << index << std::endl;
#endif
        auto * const ret = new SpendKeyData( keydata, index );
#if SPARK_DEBUGGING_OUTPUT
        std::cout << "created spend key data " << ret << " from " << to_hex_string( keydata, 32 ) << std::endl;
#endif
        return ret;
    } catch ( const std::exception &e ) {
        std::cerr << "Error in SpendKeyData construction: " << e.what() << std::endl;
        return nullptr;
    }
}

spark::SpendKey *js_createSpendKey( const SpendKeyData * const data )
{
    try {
        if ( !data ) {
            std::cerr << "Error calling createSpendKey: Provided data is null." << std::endl;
            return nullptr;
        }
        auto * const ret = new spark::SpendKey( createSpendKey( *data ) );
#if SPARK_DEBUGGING_OUTPUT
        std::cout << "createSpendKey: " << ret << ' ' << ret->get_s1() << ' ' << ret->get_s2() << " from " << data << std::endl;
#endif
        return ret;
    } catch ( const std::exception &e ) {
        std::cerr << "Error in createSpendKey: " << e.what() << std::endl;
        return nullptr;
    }
}

const char *js_getSpendKey_s1( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static std::string ret;
    ret = spend_key->get_s1().tostring();
    return ret.c_str();
}

const char *js_getSpendKey_s2( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static std::string ret;
    ret = spend_key->get_s2().tostring();
    return ret.c_str();
}

const char *js_getSpendKey_r( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static std::string ret;
    ret = spend_key->get_r().tostring();
    return ret.c_str();
}

const char *js_getSpendKey_s1_hex( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static std::string ret;
    ret = spend_key->get_s1().GetHex();
    return ret.c_str();
}

const char *js_getSpendKey_s2_hex( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static std::string ret;
    ret = spend_key->get_s2().GetHex();
    return ret.c_str();
}

const char *js_getSpendKey_r_hex( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static std::string ret;
    ret = spend_key->get_r().GetHex();
    return ret.c_str();
}

spark::FullViewKey *js_createFullViewKey( const spark::SpendKey * const spend_key )
{
    try {
        if ( !spend_key ) {
            std::cerr << "Error calling createFullViewKey: Provided SpendKey is null." << std::endl;
            return nullptr;
        }
        auto * const ret = new spark::FullViewKey( createFullViewKey( *spend_key ) );
#if SPARK_DEBUGGING_OUTPUT
        std::cout << "createFullViewKey: " << ret << ' ' << ret->get_s1() << ' ' << ret->get_P2() << " from " << spend_key << std::endl;
#endif
        return ret;
    } catch ( const std::exception &e ) {
        std::cerr << "Error in createFullViewKey: " << e.what() << std::endl;
        return nullptr;
    }
}

spark::IncomingViewKey *js_createIncomingViewKey( const spark::FullViewKey * const full_view_key )
{
   try {
      if ( !full_view_key ) {
         std::cerr << "Error calling createIncomingViewKey: Provided FullViewKey is null." << std::endl;
         return nullptr;
      }
      auto * const ret = new spark::IncomingViewKey( createIncomingViewKey( *full_view_key ) );
#if SPARK_DEBUGGING_OUTPUT
      std::cout << "createIncomingViewKey: " << ret << ' ' << ret->get_s1() << ' ' << ret->get_P2() << " from " << full_view_key << std::endl;
#endif
      return ret;
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in createIncomingViewKey: " << e.what() << std::endl;
      return nullptr;
   }
}

spark::Address *js_getAddress( const spark::IncomingViewKey * const incoming_view_key, const std::uint64_t diversifier )
{
   try {
      if ( !incoming_view_key ) {
         std::cerr << "Error calling getAddress: Provided IncomingViewKey is null." << std::endl;
         return nullptr;
      }
      return new spark::Address( getAddress( *incoming_view_key, diversifier ) );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getAddress: " << e.what() << std::endl;
      return nullptr;
   }
}

// Wrapper function to encode an Address object to a string representation, with a specified network (test or main).
const char *js_encodeAddress( const spark::Address * const address, const std::int32_t is_test_net )
{
   try {
      if ( !address ) {
         std::cerr << "Error calling encodeAddress: Provided Address is null." << std::endl;
         return nullptr;
      }
      static std::string encoded;
      encoded = address->encode( is_test_net ? spark::ADDRESS_NETWORK_TESTNET : spark::ADDRESS_NETWORK_MAINNET );
      return encoded.c_str();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in encodeAddress: " << e.what() << std::endl;
      return nullptr;
   }
}

// Wrapper function to validate whether a given string is a valid Spark address, with a specified network (test or main).
std::int32_t js_isValidSparkAddress( const char * const address, const std::int32_t is_test_net )
{
   static_assert( noexcept( isValidSparkAddress( address, !!is_test_net ) ), "isValidSparkAddress() was not supposed to ever throw" );
   return isValidSparkAddress( address, !!is_test_net );
}

// Wrapper function to decode a Spark address from its string representation, with a specified network (test or main).
spark::Address *js_decodeAddress( const char * const address_cstr, const std::int32_t is_test_net )
{
   try {
      if ( !address_cstr ) {
         std::cerr << "Error calling decodeAddress: Provided address is null." << std::endl;
         return nullptr;
      }

      auto address = std::make_unique< spark::Address >();
      const unsigned char network = address->decode( address_cstr );
      if ( network == ( is_test_net ? spark::ADDRESS_NETWORK_TESTNET : spark::ADDRESS_NETWORK_MAINNET ) )
         return address.release();
     std::cerr << "Error in decodeAddress: network mismatch." << std::endl;
     return nullptr;
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in decodeAddress: " << e.what() << std::endl;
      return nullptr;
   }
}

spark::MintedCoinData *js_createMintedCoinData( const spark::Address * const address, const std::uint64_t value, const char * const memo )
{
   try {
      if ( !address ) {
         std::cerr << "Error calling createMintedCoinData: Provided address is null." << std::endl;
         return nullptr;
      }
      if ( !memo ) {
         std::cerr << "Error calling createMintedCoinData: Provided memo is null." << std::endl;
         return nullptr;
      }
      if ( !value ) {
         std::cerr << "Error calling createMintedCoinData: Provided value is 0." << std::endl;
         return nullptr;
      }
      return new spark::MintedCoinData{ *address, value, memo };
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in createMintedCoinData: " << e.what() << std::endl;
      return nullptr;
   }
}

/*
 * Wrapper function to create Spark Mint Recipients.
 * ATTENTION: This function will perform deletion of all `outputs`, regardless of the function's outcome.
 *
 * Parameters:
 *   - outputs: Array of MintedCoinData pointers.
 *   - outputs_length: Length of the outputs array.
 *   - serial_context: Byte array containing the serial context.
 *   - serial_context_length: Length of the serial context array.
 *   - generate: Boolean indicating whether to generate the actual coin data.
 * Returns:
 *   - An address to a vector of CRecipients, or nullptr upon failure.
 */
const std::vector< CRecipient > *js_createSparkMintRecipients( const spark::MintedCoinData **outputs,
                                                               const std::uint32_t outputs_length,
                                                               const unsigned char * const serial_context,
                                                               const std::uint32_t serial_context_length,
                                                               const std::int32_t generate )
{
   BOOST_SCOPE_EXIT( &outputs, outputs_length )
   {
      for ( std::size_t i = 0; i < outputs_length; ++i )
         delete outputs[ i ];
   }
   BOOST_SCOPE_EXIT_END

   try {
      if ( !outputs || outputs_length == 0 ) {
         std::cerr << "Error calling createSparkMintRecipients: Outputs array is null or empty." << std::endl;
         return nullptr;
      }

      if ( !serial_context || serial_context_length == 0 ) {
         std::cerr << "Error calling createSparkMintRecipients: Serial context is null or empty." << std::endl;
         return nullptr;
      }

      // Convert the array of MintedCoinData pointers to a vector.
      std::vector< spark::MintedCoinData > outputs_vec;
      outputs_vec.reserve( outputs_length );
      for ( std::size_t i = 0; i < outputs_length; ++i ) {
         if ( !outputs[ i ] ) {
            std::cerr << "Error calling createSparkMintRecipients: Null pointer found in outputs array." << std::endl;
            return nullptr;
         }
         outputs_vec.emplace_back( std::move( *outputs[ i ] ) );
      }

      // Convert the serial context byte array to a vector.
      const std::vector serial_context_vec( serial_context, serial_context + serial_context_length );

      return new std::vector< CRecipient >( createSparkMintRecipients( outputs_vec, serial_context_vec, !!generate ) );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in createSparkMintRecipients: " << e.what() << std::endl;
      return nullptr;
   }
}

/*
 * Wrapper function to get the length of a vector of CRecipient.
 * This function ensures safe handling of the size retrieval process.
 *
 * Parameters:
 *   - recipients: Pointer to the vector of CRecipient.
 * Returns:
 *   - The length of the recipients vector, or -1 if null or unrepresentable in the target type.
 */
std::int32_t js_getRecipientVectorLength( const std::vector< CRecipient > * const recipients )
{
   try {
      if ( !recipients ) {
         std::cerr << "Error calling getRecipientVectorLength: Provided recipients pointer is null." << std::endl;
         return -1;
      }
      return boost::numeric_cast< std::int32_t >( recipients->size() );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getRecipientVectorLength: " << e.what() << std::endl;
      return -1;
   }
}

/*
 * Wrapper function to get a recipient at a specific index from a vector of CRecipient.
 * This function ensures safe access to the recipient data.
 *
 * Parameters:
 *   - recipients: Pointer to the vector of CRecipient.
 *   - index: The index of the recipient to retrieve.
 * Returns:
 *   - Pointer to the CRecipient at the specified index, or nullptr if out of range or error.
 */
const CRecipient *js_getRecipientAt( const std::vector< CRecipient > * const recipients, const std::int32_t index )
{
   try {
      if ( !recipients ) {
         std::cerr << "Error calling getRecipientAt: Provided recipients pointer is null." << std::endl;
         return nullptr;
      }

      if ( index < 0 || static_cast< std::size_t >( index ) >= recipients->size() ) {
         std::cerr << "Error calling getRecipientAt: Index " << index << " is out of bounds." << std::endl;
         return nullptr;
      }

      return &recipients->at( index );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getRecipientAt: " << e.what() << std::endl;
      return nullptr;
   }
}

/*
 * Wrapper function to get the raw content of the scriptPubKey data of a CRecipient.
 *
 * Parameters:
 *   - recipient: Pointer to a CRecipient.
 * Returns:
 *   - Pointer to the byte array comprising the recipient's pubKey script, or nullptr if recipient is null.
 */
const unsigned char *js_getRecipientScriptPubKey( const CRecipient * const recipient )
{
   if ( !recipient ) {
      std::cerr << "Error calling getRecipientScriptPubKey: Provided recipient pointer is null." << std::endl;
      return nullptr;
   }
   return recipient->pubKey.data();
}

/*
 * Wrapper function to get the size of the scriptPubKey data of a CRecipient.
 *
 * Parameters:
 *   - recipient: Pointer to a CRecipient.
 * Returns:
 *   - Size of the byte array comprising the recipient's pubKey script, or -1 if recipient is null or the size is unrepresentable in the target type.
 */
std::int32_t js_getRecipientScriptPubKeySize( const CRecipient * const recipient )
{
   try {
      if ( !recipient ) {
         std::cerr << "Error calling getRecipientScriptPubKeySize: Provided recipient pointer is null." << std::endl;
         return -1;
      }
      return boost::numeric_cast< std::int32_t >( recipient->pubKey.size() );
   } catch ( const std::exception &e ) {
      std::cerr << "Error in getRecipientScriptPubKeySize: " << e.what() << std::endl;
      return -1;
   }
}

/*
 * Wrapper function to get the amount of a CRecipient.
 *
 * Parameters:
 *   - recipient: Pointer to a CRecipient.
 * Returns:
 *   - The amount as std::int64_t, or -1 if recipient is null.
 */
std::int64_t js_getRecipientAmount( const CRecipient * const recipient )
{
   if ( !recipient ) {
      std::cerr << "Error calling getRecipientAmount: Provided recipient pointer is null." << std::endl;
      return -1;
   }
   return recipient->amount;
}

/*
 * Wrapper function to get the subtractFeeFromAmount flag of a CRecipient.
 *
 * Parameters:
 *   - recipient: Pointer to a CRecipient.
 * Returns:
 *   - The subtractFeeFromAmount boolean flag as std::int32_t (0/1), or 0 if recipient is null.
 */
std::int32_t js_getRecipientSubtractFeeFromAmountFlag( const CRecipient * const recipient )
{
   if ( !recipient ) {
      std::cerr << "Error calling getRecipientSubtractFeeFromAmount: Provided recipient pointer is null." << std::endl;
      return false;
   }
   return recipient->subtractFeeFromAmount;
}

/*
 * Wrapper function to deserialize a coin from its serialized byte array representation.
 *
 * Parameters:
 *   - serialized_data: Pointer to the byte array containing the serialized coin.
 *   - serialized_data_length: The size of the serialized_data array.
 * Returns:
 *   - Pointer to a dynamically allocated spark::Coin object on successful deserialization, or nullptr on failure.
 */
spark::Coin *js_deserializeCoin( const unsigned char * const serialized_data, const std::uint32_t serialized_data_length,
                                 const unsigned char * const serial_context, const std::uint32_t serial_context_length )
{
   try {
      if ( !serialized_data ) {
         std::cerr << "Error calling deserializeCoin: Provided serialized_data pointer is null." << std::endl;
         return nullptr;
      }

      return new spark::Coin( deserializeCoin( serialized_data, serialized_data_length, serial_context, serial_context_length ) );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in deserializeCoin: " << e.what() << std::endl;
      return nullptr;
   }
}

/*
 * Wrapper function to get a spark::Coin from a CSparkMintMeta object and an IncomingViewKey.
 *
 * Parameters:
 *   - meta: Pointer to the CSparkMintMeta object containing metadata for the coin.
 *   - incoming_view_key: Pointer to the spark::IncomingViewKey object.
 * Returns:
 *   - Pointer to a dynamically allocated spark::Coin object, or nullptr on failure.
 */
spark::Coin *js_getCoinFromMeta( const CSparkMintMeta * const meta, const spark::IncomingViewKey * const incoming_view_key )
{
   try {
      if ( !meta ) {
         std::cerr << "Error calling getCoinFromMeta: Provided meta pointer is null." << std::endl;
         return nullptr;
      }
      if ( !incoming_view_key ) {
         std::cerr << "Error calling getCoinFromMeta: Provided incoming_view_key pointer is null." << std::endl;
         return nullptr;
      }
      return new spark::Coin( getCoinFromMeta( *meta, *incoming_view_key ) );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getCoinFromMeta: " << e.what() << std::endl;
      return nullptr;
   }
}

/*
 * Wrapper function for getMetadata.
 *
 * Parameters:
 *   - coin: Pointer to a spark::Coin object.
 *   - incoming_view_key: Pointer to a spark::IncomingViewKey object.
 * Returns:
 *   - A dynamically allocated CSparkMintMeta on success, or nullptr on failure.
 */
CSparkMintMeta *js_getMetadata( const spark::Coin * const coin, const spark::IncomingViewKey * const incoming_view_key )
{
   try {
      if ( !coin ) {
         std::cerr << "Error calling getMetadata: Provided coin pointer is null." << std::endl;
         return nullptr;
      }
      if ( !incoming_view_key ) {
         std::cerr << "Error calling getMetadata: Provided incoming_view_key pointer is null." << std::endl;
         return nullptr;
      }
      auto meta = std::make_unique< CSparkMintMeta >( getMetadata( *coin, *incoming_view_key ) );
      static int success_count, failure_count;
      if ( meta->k.isZero() ) { // this is how getMetadata()'s failure can be detected, as currently implemented
         ++failure_count;
#if SPARK_DEBUGGING_OUTPUT
         std::cout << "Wrapped getMetadata(" << coin << ',' << incoming_view_key << ") function failed to return a proper CSparkMintMeta object; nsuccess=" << success_count << " nfailure=" << failure_count << std::endl;
#endif
         return nullptr;
      }
      ++success_count;
      // this output should be very rare in comparison to the number of failures, so perhaps it's ok to have this printed unconditionally
      std::cout << "Wrapped getMetadata(" << coin << ',' << incoming_view_key << ") function succeeded in returning a proper CSparkMintMeta; nsuccess=" << success_count << " nfailure=" << failure_count << std::endl;
      return meta.release();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getMetadata: " << e.what() << std::endl;
      return nullptr;
   }
}

/*
 * Wrapper function for getInputData (without metadata param).
 *
 * Parameters:
 *   - coin: Pointer to a spark::Coin object.
 *   - full_view_key: Pointer to a spark::FullViewKey object.
 *   - incoming_view_key: Pointer to a spark::IncomingViewKey object.
 * Returns:
 *   - A dynamically allocated spark::InputCoinData on success, or nullptr on failure.
 */
spark::InputCoinData *js_getInputData( const spark::Coin * const coin, const spark::FullViewKey * const full_view_key, const spark::IncomingViewKey * const incoming_view_key )
{
   try {
      if ( !coin ) {
         std::cerr << "Error calling getInputData: Provided coin pointer is null." << std::endl;
         return nullptr;
      }
      if ( !full_view_key ) {
         std::cerr << "Error calling getInputData: Provided full_view_key pointer is null." << std::endl;
         return nullptr;
      }
      if ( !incoming_view_key ) {
         std::cerr << "Error calling getInputData: Provided incoming_view_key pointer is null." << std::endl;
         return nullptr;
      }
      auto data = std::make_unique< spark::InputCoinData >( getInputData( *coin, *full_view_key, *incoming_view_key ) );
      if ( data->k.isZero() ) { // this is how getInputData()'s failure can be detected, as currently implemented
         std::cerr << "Wrapped getInputData() function failed to return a proper InputCoinData object." << std::endl;
         return nullptr;
      }
      return data.release();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getInputData: " << e.what() << std::endl;
      return nullptr;
   }
}

/*
 * Wrapper function for getInputData with metadata.
 *
 * Parameters:
 *   - coin: Pointer to a spark::Coin object.
 *   - coin: Pointer to the corresponding CSparkMintMeta object.
 *   - full_view_key: Pointer to a spark::FullViewKey object.
 * Returns:
 *   - A dynamically allocated spark::InputCoinData on success, or nullptr on failure.
 */
spark::InputCoinData *js_getInputDataWithMeta( const spark::Coin * const coin, const CSparkMintMeta * const meta, const spark::FullViewKey * const full_view_key )
{
   try {
      if ( !coin ) {
         std::cerr << "Error calling getInputDataWithMeta: Provided coin pointer is null." << std::endl;
         return nullptr;
      }
      if ( !meta ) {
         std::cerr << "Error calling getInputDataWithMeta: Provided meta pointer is null." << std::endl;
         return nullptr;
      }
      if ( !full_view_key ) {
         std::cerr << "Error calling getInputDataWithMeta: Provided full_view_key pointer is null." << std::endl;
         return nullptr;
      }
      return new spark::InputCoinData( getInputData( std::pair( *coin, *meta ), *full_view_key ) );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getInputDataWithMeta: " << e.what() << std::endl;
      return nullptr;
   }
}

/*
 * Wrapper function for identifyCoin.
 *
 * Parameters:
 *   - coin: Pointer to a spark::Coin object.
 *   - incoming_view_key: Pointer to a spark::IncomingViewKey object.
 * Returns:
 *   - A dynamically allocated spark::IdentifiedCoinData on success, or nullptr on failure.
 */
spark::IdentifiedCoinData *js_identifyCoin( const spark::Coin * const coin, const spark::IncomingViewKey * const incoming_view_key )
{
   try {
      if ( !coin ) {
         std::cerr << "Error calling identifyCoin: Provided coin pointer is null." << std::endl;
         return nullptr;
      }
      if ( !incoming_view_key ) {
         std::cerr << "Error calling identifyCoin: Provided incoming_view_key pointer is null." << std::endl;
         return nullptr;
      }
      return new spark::IdentifiedCoinData( identifyCoin( *coin, *incoming_view_key ) );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in identifyCoin: " << e.what() << std::endl;
      return nullptr;
   }
}

// Access integral attribute "i" (diversifier) from IdentifiedCoinData
std::uint64_t js_getIdentifiedCoinDiversifier( const spark::IdentifiedCoinData * const identified_coin_data )
{
   if ( !identified_coin_data ) {
      std::cerr << "Error in getIdentifiedCoinDiversifier: Provided IdentifiedCoinData pointer is null." << std::endl;
      return 0;   // Return 0 to indicate an error
   }
   return identified_coin_data->i;
}

// Access integral attribute "v" (value) from IdentifiedCoinData
std::uint64_t js_getIdentifiedCoinValue( const spark::IdentifiedCoinData * const identified_coin_data )
{
   if ( !identified_coin_data ) {
      std::cerr << "Error in getIdentifiedCoinValue: Provided IdentifiedCoinData pointer is null." << std::endl;
      return 0;   // Return 0 to indicate an error
   }
   return identified_coin_data->v;
}

// Access string attribute "memo" from IdentifiedCoinData
const char *js_getIdentifiedCoinMemo( const spark::IdentifiedCoinData * const identified_coin_data )
{
   if ( !identified_coin_data ) {
      std::cerr << "Error in getIdentifiedCoinMemo: Provided IdentifiedCoinData pointer is null." << std::endl;
      return nullptr;   // Return null to indicate an error
   }
   return identified_coin_data->memo.c_str();
}

// Access integral attribute "nHeight" from CSparkMintMeta
std::int32_t js_getCSparkMintMetaHeight( const CSparkMintMeta * const meta )
{
   if ( !meta ) {
      std::cerr << "Error in getCSparkMintMetaHeight: Provided CSparkMintMeta pointer is null." << std::endl;
      return -1;   // Return -1 to indicate an error
   }
   return meta->nHeight;
}

// Access integral attribute "nId" from CSparkMintMeta
std::int32_t js_getCSparkMintMetaId( const CSparkMintMeta * const meta )
{
   if ( !meta ) {
      std::cerr << "Error in getCSparkMintMetaId: Provided CSparkMintMeta pointer is null." << std::endl;
      return -1;   // Return -1 to indicate an error
   }
   return meta->nId;
}

// Access boolean attribute "isUsed" from CSparkMintMeta, as an int
std::int32_t js_getCSparkMintMetaIsUsed( const CSparkMintMeta * const meta )
{
   if ( !meta ) {
      std::cerr << "Error in getCSparkMintMetaIsUsed: Provided CSparkMintMeta pointer is null." << std::endl;
      return false;   // Return false to indicate an error
   }
   return meta->isUsed;
}

// Access string attribute "memo" from CSparkMintMeta
const char *js_getCSparkMintMetaMemo( const CSparkMintMeta * const meta )
{
   if ( !meta ) {
      std::cerr << "Error in getCSparkMintMetaMemo: Provided CSparkMintMeta pointer is null." << std::endl;
      return nullptr;   // Return null to indicate an error
   }
   return meta->memo.c_str();
}

// Access Scalar attribute "k" (nonce) from CSparkMintMeta
const char *js_getCSparkMintMetaNonce( const CSparkMintMeta *const meta )
{
   if ( !meta ) {
      std::cerr << "Error in getCSparkMintMetaNonce: Provided CSparkMintMeta pointer is null." << std::endl;
      return nullptr;   // Return null to indicate an error
   }
   static std::string k;
   k = meta->k.GetHex();
   return k.c_str();
}

// Access integral attribute "i" (diversifier) from CSparkMintMeta
std::uint64_t js_getCSparkMintMetaDiversifier( const CSparkMintMeta * const meta )
{
   if ( !meta ) {
      std::cerr << "Error in getCSparkMintMetaDiversifier: Provided CSparkMintMeta pointer is null." << std::endl;
      return 0;   // Return 0 to indicate an error
   }
   return meta->i;
}

// Access integral attribute "v" (value) from CSparkMintMeta
std::uint64_t js_getCSparkMintMetaValue( const CSparkMintMeta * const meta )
{
   if ( !meta ) {
      std::cerr << "Error in getCSparkMintMetaValue: Provided CSparkMintMeta pointer is null." << std::endl;
      return 0;   // Return 0 to indicate an error
   }
   return meta->v;
}

// Access char attribute "type" from CSparkMintMeta, as an int
std::int32_t js_getCSparkMintMetaType( const CSparkMintMeta * const meta )
{
   if ( !meta ) {
      std::cerr << "Error in getCSparkMintMetaType: Provided CSparkMintMeta pointer is null." << std::endl;
      return 0;   // Return 0 to indicate an error
   }
   return meta->type;
}

// Access Coin attribute "coin" from CSparkMintMeta
const spark::Coin *js_getCSparkMintMetaCoin( const CSparkMintMeta * const meta )
{
   if ( !meta ) {
      std::cerr << "Error in getCSparkMintMetaCoin: Provided CSparkMintMeta pointer is null." << std::endl;
      return nullptr;   // Return null to indicate an error
   }
   return &meta->coin;
}

// Set integral attribute "nId" for CSparkMintMeta
void js_setCSparkMintMetaId( CSparkMintMeta * const meta, const std::int32_t id )
{
   if ( !meta ) {
      std::cerr << "Error in setCSparkMintMetaId: Provided CSparkMintMeta pointer is null." << std::endl;
      return;
   }
   meta->nId = id;
}

// Set integral attribute "nHeight" for CSparkMintMeta
void js_setCSparkMintMetaHeight( CSparkMintMeta * const meta, const std::int32_t height )
{
   if ( !meta ) {
      std::cerr << "Error in setCSparkMintMetaHeight: Provided CSparkMintMeta pointer is null." << std::endl;
      return;
   }
   meta->nHeight = height;
}

// Get hash of a spark::Coin
const char *js_getCoinHash( const spark::Coin * const coin )
{
   try {
      if ( !coin ) {
         std::cerr << "Error in getCoinHash: Provided Coin pointer is null." << std::endl;
         return nullptr;
      }
      static std::string hash;
      hash = coin->getHash().GetHex();
      return hash.c_str();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getCoinHash: " << e.what() << std::endl;
      return nullptr;
   }
}

// Access integral attribute "cover_set_id" from InputCoinData
std::uint64_t js_getInputCoinDataCoverSetId( const spark::InputCoinData * const input_coin_data )
{
   if ( !input_coin_data ) {
      std::cerr << "Error in getInputCoinDataCoverSetId: Provided InputCoinData pointer is null." << std::endl;
      return 0;   // Return 0 to indicate an error
   }
   return input_coin_data->cover_set_id;
}

// Access integral attribute "index" from InputCoinData
std::uint64_t js_getInputCoinDataIndex( const spark::InputCoinData * const input_coin_data )
{
   if ( !input_coin_data ) {
      std::cerr << "Error in getInputCoinDataIndex: Provided InputCoinData pointer is null." << std::endl;
      return 0;   // Return 0 to indicate an error
   }
   return input_coin_data->index;
}

// Access value attribute "v" from InputCoinData
std::uint64_t js_getInputCoinDataValue( const spark::InputCoinData * const input_coin_data )
{
   if ( !input_coin_data ) {
      std::cerr << "Error in getInputCoinDataValue: Provided InputCoinData pointer is null." << std::endl;
      return 0;   // Return 0 to indicate an error
   }
   return input_coin_data->v;
}

// Access attribute "T" from InputCoinData representation as hex string
const char *js_getInputCoinDataTag_hex( const spark::InputCoinData * const input_coin_data )
{
   if ( !input_coin_data ) {
      std::cerr << "Error in getInputCoinDataTag_hex: Provided InputCoinData pointer is null." << std::endl;
      return nullptr;
   }
   static std::string tag_hex;
   tag_hex = input_coin_data->T.GetHex();
   return tag_hex.c_str();
}

// Access attribute "T" from InputCoinData, serialized then converted to base64
const char *js_getInputCoinDataTag_base64( const spark::InputCoinData * const input_coin_data )
{
   if ( !input_coin_data ) {
      std::cerr << "Error in getInputCoinDataTag_base64: Provided InputCoinData pointer is null." << std::endl;
      return nullptr;
   }
   std::array< unsigned char, 34 > bytes;
   input_coin_data->T.serialize( bytes.data() );
   static std::string tag_base64;
   tag_base64 = EncodeBase64( bytes.data(), bytes.size() );
   return tag_base64.c_str();
}

// Create vector for `recipients` used in createSparkSpendTransaction
std::vector< std::pair< CAmount, bool > > *js_createRecipientsVectorForCreateSparkSpendTransaction( std::int32_t intended_final_size )
{
   try {
      auto v = std::make_unique< std::vector< std::pair< CAmount, bool > > >();
      if ( intended_final_size > 0 )
         v->reserve( intended_final_size );
      return v.release();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in createRecipientsVectorForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return nullptr;
   }
}

// Add recipient to vector used in createSparkSpendTransaction
std::int32_t js_addRecipientForCreateSparkSpendTransaction( std::vector< std::pair< CAmount, bool > > * const recipients, const CAmount amount, const std::int32_t subtract_fee_from_amount )
{
   try {
      if ( !recipients ) {
         std::cerr << "Error in addRecipientForCreateSparkSpendTransaction: Provided recipients pointer is null." << std::endl;
         return false;
      }
      recipients->emplace_back( amount, !!subtract_fee_from_amount );
      return true;
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in addRecipientForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return false;
   }
}

// Create vector for private recipients used in createSparkSpendTransaction
std::vector< std::pair< spark::OutputCoinData, bool > > *js_createPrivateRecipientsVectorForCreateSparkSpendTransaction( std::int32_t intended_final_size )
{
   try {
      auto v = std::make_unique< std::vector< std::pair< spark::OutputCoinData, bool > > >();
      if ( intended_final_size > 0 )
         v->reserve( intended_final_size );
      return v.release();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in createPrivateRecipientsVectorForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return nullptr;
   }
}

// Add private recipient to vector used in createSparkSpendTransaction
std::int32_t js_addPrivateRecipientForCreateSparkSpendTransaction( std::vector< std::pair< spark::OutputCoinData, bool > > * const private_recipients,
                                                                   const spark::Address * const address,
                                                                   const std::uint64_t value,
                                                                   const char * const memo,
                                                                   const std::int32_t subtract_fee_from_amount )
{
   try {
      if ( !private_recipients ) {
         std::cerr << "Error in addPrivateRecipientForCreateSparkSpendTransaction: Provided private_recipients pointer is null." << std::endl;
         return false;
      }
      if ( !address ) {
         std::cerr << "Error in addPrivateRecipientForCreateSparkSpendTransaction: Provided address pointer is null." << std::endl;
         return false;
      }
      if ( !memo ) {
         std::cerr << "Error in addPrivateRecipientForCreateSparkSpendTransaction: Provided memo pointer is null." << std::endl;
         return false;
      }
      private_recipients->emplace_back( spark::OutputCoinData{ *address, value, memo }, !!subtract_fee_from_amount );
      return true;
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in addPrivateRecipientForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return false;
   }
}

// Create list of coins for createSparkSpendTransaction
std::list< CSparkMintMeta > *js_createCoinsListForCreateSparkSpendTransaction()
{
   try {
      return new std::list< CSparkMintMeta >();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in createCoinsListForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return nullptr;
   }
}

// Add coin to coins list used in createSparkSpendTransaction
std::int32_t js_addCoinToListForCreateSparkSpendTransaction( std::list< CSparkMintMeta > * const coins, const CSparkMintMeta * const coin )
{
   try {
      if ( !coins ) {
         std::cerr << "Error in addCoinToListForCreateSparkSpendTransaction: Provided coins list pointer is null." << std::endl;
         return false;
      }
      if ( !coin ) {
         std::cerr << "Error in addCoinToListForCreateSparkSpendTransaction: Provided coin pointer is null." << std::endl;
         return false;
      }
      coins->push_back( *coin );
      return true;
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in addCoinToListForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return false;
   }
}

spark::CoverSetData *js_createCoverSetData( const unsigned char * const representation, std::uint32_t representation_length )
{
   try {
      if ( !representation ) {
         std::cerr << "Error in createCoverSetData: Provided representation pointer is null." << std::endl;
         return nullptr;
      }
      if ( std::all_of( representation, representation + representation_length, []( const unsigned char &c ) { return c == 0; } ) )
         representation_length = 0;
      return new spark::CoverSetData{ {}, { representation, representation + representation_length } };
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in createCoverSetData: " << e.what() << std::endl;
      return nullptr;
   }
}

std::int32_t js_addCoinToCoverSetData( spark::CoverSetData * const cover_set_data, const spark::Coin * const coin )
{
   try {
      if ( !cover_set_data ) {
         std::cerr << "Error in addCoinToCoverSetData: Provided cover_set_data pointer is null." << std::endl;
         return false;
      }
      if ( !coin ) {
         std::cerr << "Error in addCoinToCoverSetData: Provided coin pointer is null." << std::endl;
         return false;
      }
      cover_set_data->cover_set.push_back( *coin );
      return true;
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in addCoinToCoverSetData: " << e.what() << std::endl;
      return false;
   }
}

std::unordered_map< std::uint64_t, spark::CoverSetData > *js_createCoverSetDataMapForCreateSparkSpendTransaction()
{
   try {
      return new std::unordered_map< std::uint64_t, spark::CoverSetData >();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in createCoverSetDataMapForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return nullptr;
   }
}

std::int32_t js_addCoverSetDataForCreateSparkSpendTransaction( std::unordered_map< std::uint64_t, spark::CoverSetData > * const cover_set_data_map,
                                                               const std::uint64_t group_id, const spark::CoverSetData * const data )
{
   try {
      if ( !cover_set_data_map ) {
         std::cerr << "Error in addCoverSetDataForCreateSparkSpendTransaction: Provided cover_set_data pointer is null." << std::endl;
         return false;
      }
      if ( !data ) {
         std::cerr << "Error in addCoverSetDataForCreateSparkSpendTransaction: Provided data pointer is null." << std::endl;
         return false;
      }
      cover_set_data_map->emplace( group_id, *data );
      return true;
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in addCoverSetDataForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return false;
   }
}

std::int32_t js_moveAddCoverSetDataForCreateSparkSpendTransaction( std::unordered_map< std::uint64_t, spark::CoverSetData > * const cover_set_data_map,
                                                                   const std::uint64_t group_id, spark::CoverSetData * const data )
{
   try {
      if ( !cover_set_data_map ) {
         std::cerr << "Error in moveAddCoverSetDataForCreateSparkSpendTransaction: Provided cover_set_data pointer is null." << std::endl;
         return false;
      }
      if ( !data ) {
         std::cerr << "Error in moveAddCoverSetDataForCreateSparkSpendTransaction: Provided data pointer is null." << std::endl;
         return false;
      }
      cover_set_data_map->emplace( group_id, std::move( *data ) );
      return true;
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in moveAddCoverSetDataForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return false;
   }
}

std::map< std::uint64_t, uint256 > *js_createIdAndBlockHashesMapForCreateSparkSpendTransaction()
{
   try {
      return new std::map< std::uint64_t, uint256 >();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in createIdAndBlockHashesMapForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return nullptr;
   }
}

std::int32_t js_addIdAndBlockHashForCreateSparkSpendTransaction( std::map< std::uint64_t, uint256 > * const id_and_block_hashes_map,
                                                                 const std::uint64_t group_id, const char * const block_hash )
{
   try {
      if ( !id_and_block_hashes_map ) {
         std::cerr << "Error in addIdAndBlockHashForCreateSparkSpendTransaction: Provided id_and_block_hashes_map pointer is null." << std::endl;
         return false;
      }
      if ( !block_hash ) {
         std::cerr << "Error in addIdAndBlockHashForCreateSparkSpendTransaction: Provided block_hash pointer is null." << std::endl;
         return false;
      }
      id_and_block_hashes_map->emplace( group_id, uint256S( block_hash ) );
      return true;
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in addIdAndBlockHashForCreateSparkSpendTransaction: " << e.what() << std::endl;
      return false;
   }
}

struct CreateSparkSpendTxResult {
   std::vector< uint8_t > serialized_spend;
   std::vector< std::vector< unsigned char > > output_scripts;
   std::vector< CSparkMintMeta > spent_coins;
   CAmount fee;
};

CreateSparkSpendTxResult *js_createSparkSpendTransaction( const spark::SpendKey * const spend_key,
                                                          const spark::FullViewKey * const full_view_key,
                                                          const spark::IncomingViewKey * const incoming_view_key,
                                                          const std::vector< std::pair< CAmount, bool > > * const recipients,
                                                          const std::vector< std::pair< spark::OutputCoinData, bool > > * const private_recipients,
                                                          const std::list< CSparkMintMeta > *const coins,
                                                          const std::unordered_map< std::uint64_t, spark::CoverSetData > * const cover_set_data_all,
                                                          const std::map< std::uint64_t, uint256 > * const id_and_block_hashes_all,
                                                          const char * const tx_hash_hex_string,
                                                          const std::int32_t additional_tx_size )
{
   try {
      if ( !spend_key || !full_view_key || !incoming_view_key || !recipients || !private_recipients ||
           !coins || !cover_set_data_all || !id_and_block_hashes_all || !tx_hash_hex_string ) {
         std::cerr << "Error calling createSparkSpendTransaction: One or more required pointers are null" << std::endl;
         return nullptr;
      }

#if 0
      if ( tx_hash_sig_bufsize <= 0 ) {
         std::cerr << "Error calling createSparkSpendTransaction: tx_hash_sig_bufsize must be positive" << std::endl;
         return nullptr;
      }
#endif

      if ( additional_tx_size < 0 ) {
         std::cerr << "Error calling createSparkSpendTransaction: additional_tx_size cannot be negative" << std::endl;
         return nullptr;
      }

#if SPARK_DEBUGGING_OUTPUT
      std::cout << "createSparkSpendTransaction called with:"
                   "\nspend_key = " << spend_key << ' ' << spend_key->get_s1().GetHex() << ' ' << spend_key->get_s2().GetHex() << ' ' << spend_key->get_r().GetHex()
                << "\nfull_view_key = " << full_view_key << ' ' << full_view_key->get_s1().GetHex() << ' ' << full_view_key->get_s2().GetHex()
                                        << ' ' << full_view_key->get_D().GetHex() << ' ' << full_view_key->get_P2().GetHex()
                << "\nincoming_view_key = " << incoming_view_key << ' ' << incoming_view_key->get_s1().GetHex() << ' ' << incoming_view_key->get_P2().GetHex()
                << "\nrecipients = " << container_streamer( *recipients )
                << "\nprivate_recipients = " << container_streamer( *private_recipients )
                << "\ncoins = " << container_streamer( *coins )
                << "\ncover_set_data_all = " << container_streamer( *cover_set_data_all )
                << "\nid_and_block_hashes_all = " << container_streamer( *id_and_block_hashes_all )
                << "\ntx_hash_sig = " << tx_hash_hex_string// to_hex_string( tx_hash_sig_buf, tx_hash_sig_bufsize )
                << "\nadditional_tx_size = " << additional_tx_size << std::endl;
#endif

      auto result = std::make_unique< CreateSparkSpendTxResult >();
      createSparkSpendTransaction( *spend_key, *full_view_key, *incoming_view_key, *recipients, *private_recipients, *coins,
                                   *cover_set_data_all, *id_and_block_hashes_all,
                                   uint256S( tx_hash_hex_string ), additional_tx_size,
                                   result->fee, result->serialized_spend, result->output_scripts, result->spent_coins );
      return result.release();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in createSparkSpendTransaction: " << e.what() << std::endl;
      return nullptr;
   }
}

const unsigned char *js_getCreateSparkSpendTxResultSerializedSpend( const CreateSparkSpendTxResult * const result )
{
   if ( !result ) {
      std::cerr << "Error in getCreateSparkSpendTxResultSerializedSpend: Provided CreateSparkSpendTxResult pointer is null." << std::endl;
      return nullptr;
   }
   return result->serialized_spend.data();
}

std::int32_t js_getCreateSparkSpendTxResultSerializedSpendSize( const CreateSparkSpendTxResult * const result )
{
   try {
      if ( !result ) {
         std::cerr << "Error in getCreateSparkSpendTxResultSerializedSpendSize: Provided CreateSparkSpendTxResult pointer is null." << std::endl;
         return -1;
      }
      return boost::numeric_cast< std::int32_t >( result->serialized_spend.size() );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getCreateSparkSpendTxResultSerializedSpendSize: " << e.what() << std::endl;
      return -1;
   }
}

std::int32_t js_getCreateSparkSpendTxResultOutputScriptsSize( const CreateSparkSpendTxResult * const result )
{
   try {
      if ( !result ) {
         std::cerr << "Error in getCreateSparkSpendTxResultOutputScriptsSize: Provided CreateSparkSpendTxResult pointer is null." << std::endl;
         return -1;
      }
      return boost::numeric_cast< std::int32_t >( result->output_scripts.size() );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getCreateSparkSpendTxResultOutputScriptsSize: " << e.what() << std::endl;
      return -1;
   }
}

const unsigned char *js_getCreateSparkSpendTxResultOutputScriptAt( const CreateSparkSpendTxResult *const result, const std::int32_t index )
{
   try {
      if ( !result ) {
         std::cerr << "Error in getCreateSparkSpendTxResultOutputScriptAt: Provided CreateSparkSpendTxResult pointer is null." << std::endl;
         return nullptr;
      }
      if ( index < 0 || static_cast< std::size_t >( index ) >= result->output_scripts.size() ) {
         std::cerr << "Error in getCreateSparkSpendTxResultOutputScriptAt: Index " << index << " is out of bounds." << std::endl;
         return nullptr;
      }
      return result->output_scripts[ index ].data();
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getCreateSparkSpendTxResultOutputScriptAt: " << e.what() << std::endl;
      return nullptr;
   }
}

std::int32_t js_getCreateSparkSpendTxResultOutputScriptSizeAt( const CreateSparkSpendTxResult *const result, const std::int32_t index )
{
   try {
      if ( !result ) {
         std::cerr << "Error in getCreateSparkSpendTxResultOutputScriptSizeAt: Provided CreateSparkSpendTxResult pointer is null." << std::endl;
         return -1;
      }
      if ( index < 0 || static_cast< std::size_t >( index ) >= result->output_scripts.size() ) {
         std::cerr << "Error in getCreateSparkSpendTxResultOutputScriptSizeAt: Index " << index << " is out of bounds." << std::endl;
         return -1;
      }
      return boost::numeric_cast< std::int32_t >( result->output_scripts[ index ].size() );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getCreateSparkSpendTxResultOutputScriptSizeAt: " << e.what() << std::endl;
      return -1;
   }
}

std::int32_t js_getCreateSparkSpendTxResultSpentCoinsSize( const CreateSparkSpendTxResult * const result )
{
   try {
      if ( !result ) {
         std::cerr << "Error in getCreateSparkSpendTxResultSpentCoinsSize: Provided CreateSparkSpendTxResult pointer is null." << std::endl;
         return -1;
      }
      return boost::numeric_cast< std::int32_t >( result->spent_coins.size() );
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getCreateSparkSpendTxResultSpentCoinsSize: " << e.what() << std::endl;
      return -1;
   }
}

const CSparkMintMeta *js_getCreateSparkSpendTxResultSpentCoinAt( const CreateSparkSpendTxResult * const result, const std::int32_t index )
{
   try {
      if ( !result ) {
         std::cerr << "Error in getCreateSparkSpendTxResultSpentCoinAt: Provided CreateSparkSpendTxResult pointer is null." << std::endl;
         return nullptr;
      }
      if ( index < 0 || static_cast< std::size_t >( index ) >= result->spent_coins.size() ) {
         std::cerr << "Error in getCreateSparkSpendTxResultSpentCoinAt: Index " << index << " is out of bounds." << std::endl;
         return nullptr;
      }
      return &result->spent_coins[ index ];
   }
   catch ( const std::exception &e ) {
      std::cerr << "Error in getCreateSparkSpendTxResultSpentCoinAt: " << e.what() << std::endl;
      return nullptr;
   }
}

std::int64_t js_getCreateSparkSpendTxResultFee( const CreateSparkSpendTxResult * const result )
{
   if ( !result ) {
      std::cerr << "Error in getCreateSparkSpendTxResultFee: Provided CreateSparkSpendTxResult pointer is null." << std::endl;
      return -1;
   }
   return result->fee;
}

// Free memory allocated for SpendKeyData
void js_freeSpendKeyData( SpendKeyData *spend_key_data )
{
    delete spend_key_data;
}

// Free memory allocated for SpendKey
void js_freeSpendKey( spark::SpendKey *spend_key )
{
    delete spend_key;
}

// Free memory allocated for FullViewKey
void js_freeFullViewKey( spark::FullViewKey *full_view_key )
{
    delete full_view_key;
}

// Free memory allocated for IncomingViewKey
void js_freeIncomingViewKey( spark::IncomingViewKey *incoming_view_key )
{
   delete incoming_view_key;
}

// Free memory allocated for Address
void js_freeAddress( spark::Address *address )
{
   delete address;
}

// Free memory allocated for a vector of CRecipient
void js_freeRecipientVector( const std::vector< CRecipient > *recipients )
{
    delete recipients;
}

// Free memory allocated for CSparkMintMeta
void js_freeCSparkMintMeta( CSparkMintMeta *meta )
{
   delete meta;
}

// Free memory allocated for spark::InputCoinData
void js_freeInputCoinData( spark::InputCoinData *input_coin_data )
{
   delete input_coin_data;
}

// Free memory allocated for spark::IdentifiedCoinData
void js_freeIdentifiedCoinData( spark::IdentifiedCoinData *identified_coin_data )
{
   delete identified_coin_data;
}

// Free memory allocated for spark::Coin
void js_freeCoin( spark::Coin *coin )
{
   delete coin;
}

// Free memory allocated for recipients vector used in createSparkSpendTransaction
void js_freeSparkSpendRecipientsVector( std::vector< std::pair< CAmount, bool > > *recipients )
{
   delete recipients;
}

// Free memory allocated for private recipients vector used in createSparkSpendTransaction
void js_freeSparkSpendPrivateRecipientsVector( std::vector< std::pair< spark::OutputCoinData, bool > > *private_recipients )
{
   delete private_recipients;
}

// Free memory allocated for coins list used in createSparkSpendTransaction
void js_freeSparkSpendCoinsList( std::list< CSparkMintMeta > *coins )
{
   delete coins;
}

void js_freeCoverSetData( spark::CoverSetData *cover_set_data )
{
   delete cover_set_data;
}

void js_freeCoverSetDataMapForCreateSparkSpendTransaction( std::unordered_map< std::uint64_t, spark::CoverSetData > * const cover_set_data_map )
{
   delete cover_set_data_map;
}

void js_freeIdAndBlockHashesMap( std::map< std::uint64_t, uint256 > * const map )
{
   delete map;
}

// Free memory allocated for CreateSparkSpendTxResult
void js_freeCreateSparkSpendTxResult( CreateSparkSpendTxResult *result )
{
   delete result;
}

}  // extern "C"

#endif   // __EMSCRIPTEN__
