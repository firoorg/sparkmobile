#include "../include/spark.h"
#include "spark.h"

//#define __EMSCRIPTEN__ 1   // useful to uncomment - just for development (when building, needs to be defined project-wide, not just in this file)
#ifdef __EMSCRIPTEN__
#include <iostream>
#include <boost/scope_exit.hpp>
#include <boost/numeric/conversion/cast.hpp>
#endif

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
        std::size_t mintNum) {
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

        // 924 is constant part, mainly Schnorr and Range proofs, 2535 is for each grootle proof/aux data
        // 213 for each private output, 144 other parts of tx,
        size = 924 + 2535 * (spendCoins.size()) + 213 * mintNum + 144; //TODO (levon) take in account also utxoNum
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
            SelectSparkCoins(vOut + mintVOut, recipientsToSubtractFee, coins, privateRecipients.size());

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
        throw std::runtime_error("Invalid pend key.");

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

    std::vector<spark::InputCoinData> inputs;
    std::map<uint64_t, uint256> idAndBlockHashes;
    std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data;
    for (auto& coin : estimated.second) {
        uint64_t groupId = coin.nId;
        if (cover_set_data.count(groupId) == 0) {
            if (!(cover_set_data_all.count(groupId) > 0 && idAndBlockHashes_all.count(groupId) > 0 ))
                throw std::runtime_error("No such coin in set in input data");
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

spark::Address getAddress(const spark::IncomingViewKey& incomingViewKey, const uint64_t diversifier)
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

    return meta;
}

spark::InputCoinData getInputData(spark::Coin coin, const spark::FullViewKey& full_view_key, const spark::IncomingViewKey& incoming_view_key)
{
    spark::InputCoinData inputCoinData;
    spark::IdentifiedCoinData identifiedCoinData;
    try {
        identifiedCoinData = identifyCoin(coin, incoming_view_key);
    } catch (...) {
        return inputCoinData;
    }

    spark::RecoveredCoinData recoveredCoinData = coin.recover(full_view_key, identifiedCoinData);
    inputCoinData.T = recoveredCoinData.T;
    inputCoinData.s = recoveredCoinData.s;
    inputCoinData.k = identifiedCoinData.k;
    inputCoinData.v = identifiedCoinData.v;

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

#ifdef __EMSCRIPTEN__

extern "C" {

SpendKeyData *js_createSpendKeyData( const unsigned char * const keydata, std::int32_t index )
{
    try {
        if ( !keydata ) {
           std::cerr << "Error calling createSpendKeyData: Provided keydata is null." << std::endl;
           return nullptr;
        }
        return new SpendKeyData( keydata, index );
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
        return new spark::SpendKey( createSpendKey( *data ) );
    } catch ( const std::exception &e ) {
        std::cerr << "Error in createSpendKey: " << e.what() << std::endl;
        return nullptr;
    }
}

const char *js_getSpendKey_s1( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static const std::string ret = spend_key->get_s1().tostring();
    return ret.c_str();
}

const char *js_getSpendKey_s2( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static const std::string ret = spend_key->get_s2().tostring();
    return ret.c_str();
}

const char *js_getSpendKey_r( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static const std::string ret = spend_key->get_r().tostring();
    return ret.c_str();
}

const char *js_getSpendKey_s1_hex( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static const std::string ret = spend_key->get_s1().GetHex();
    return ret.c_str();
}

const char *js_getSpendKey_s2_hex( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static const std::string ret = spend_key->get_s2().GetHex();
    return ret.c_str();
}

const char *js_getSpendKey_r_hex( const spark::SpendKey * const spend_key )
{
    if ( !spend_key )
       return nullptr;
    static const std::string ret = spend_key->get_r().GetHex();
    return ret.c_str();
}
   
spark::FullViewKey *js_createFullViewKey( const spark::SpendKey * const spend_key )
{
    try {
        if ( !spend_key ) {
            std::cerr << "Error calling createFullViewKey: Provided SpendKey is null." << std::endl;
            return nullptr;
        }
        return new spark::FullViewKey( createFullViewKey( *spend_key ) );
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
      return new spark::IncomingViewKey( createIncomingViewKey( *full_view_key ) );
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
      static const std::string encoded = address->encode( is_test_net ? spark::ADDRESS_NETWORK_TESTNET : spark::ADDRESS_NETWORK_MAINNET );
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
spark::Address *js_decodeAddress( const char *address_cstr, const std::int32_t is_test_net )
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
                                                               const unsigned char *serial_context,
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
   
}  // extern "C"

#endif   // __EMSCRIPTEN__