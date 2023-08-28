#ifndef SPARK_H
#define SPARK_H

#include "../bitcoin/script.h"
#include "../bitcoin/amount.h"
#include "../src/primitives.h"
#include "../src/coin.h"
#include "../src/mint_transaction.h"
#include "../src/spend_transaction.h"
#include <list>
//#include <string>
//

const uint32_t DEFAULT_SPARK_NCOUNT = 1;
#define SPARK_OUT_LIMIT_PER_TX         16
#define SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION     (10000 * COIN)
// Spend input scrypt opcode
#define OP_SPARKSPEND 0xd3
// Transaction type (nType)
#define TRANSACTION_SPARK 9
// Spark spend version (nVersion)
#define SPARK_TX_VERSION 3
//Diversifier for spark change address,
const uint32_t SPARK_CHANGE_D = 0x270F;

struct CRecipient
{
    CScript pubKey;
    CAmount amount;
    bool subtractFeeFromAmount;
};

class SpendKeyData {
public:
    SpendKeyData(unsigned char* keydata, int32_t index = DEFAULT_SPARK_NCOUNT) {
        memcpy(this->keydata, keydata, 32);
        this->index = index;
    }

    const unsigned char* getKeyData() const { return keydata; }
    const int32_t getIndex() const {return index; }
    unsigned int size() const { return 32; }

private:
    unsigned char keydata[32];
    int32_t index;
};

spark::SpendKey createSpendKey(const SpendKeyData& data);
spark::FullViewKey createFullViewKey(const spark::SpendKey& spendKey);
spark::IncomingViewKey createIncomingViewKey(const spark::FullViewKey& fullViewKey);

std::vector<CRecipient> CreateSparkMintRecipients(const std::vector<spark::MintedCoinData>& outputs, const std::vector<unsigned char>& serial_context, bool generate);

std::pair<CAmount, std::vector<CSparkMintMeta>> SelectSparkCoins(CAmount required, bool subtractFeeFromAmount, std::list<CSparkMintMeta> coins, std::size_t mintNum);
bool GetCoinsToSpend(
        CAmount required,
        std::vector<CSparkMintMeta>& coinsToSpend_out,
        std::list<CSparkMintMeta> coins,
        int64_t& changeToMint);
spark::Address getAddress(const spark::IncomingViewKey& incomingViewKey, const uint64_t diversifier);

void ParseSparkMintTransaction(const std::vector<CScript>& scripts, spark::MintTransaction& mintTransaction);
void ParseSparkMintCoin(const CScript& script, spark::Coin& txCoin);

void getSparkSpendScripts(const spark::FullViewKey& fullViewKey,
                          const spark::SpendKey& spendKey,
                          const std::vector<spark::InputCoinData>& inputs,
                          const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data,
                          const std::map<uint64_t, uint256>& idAndBlockHashes,
                          CAmount fee,
                          uint64_t transparentOut,
                          const std::vector<spark::OutputCoinData>& privOutputs,
                          std::vector<uint8_t>& inputScript, std::vector<std::vector<unsigned char>>& outputScripts);

void CreateSparkSpendTransaction(
        const spark::SpendKey& spendKey,
        const spark::FullViewKey& fullViewKey,
        const spark::IncomingViewKey& incomingViewKey,
        const std::vector<std::pair<CAmount, bool>>& recipients,
        const std::vector<std::pair<spark::OutputCoinData, bool>>& privateRecipients,
        std::list<CSparkMintMeta> coins,
        const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data_all,
        const std::map<uint64_t, uint256>& idAndBlockHashes_all,
        const uint256& txHashSig,
        CAmount &fee,
        std::vector<uint8_t>& serializedSpend,
        std::vector<std::vector<unsigned char>>& outputScripts);

// CSparkMintMeta is defined at ../src/primitives.h
spark::Coin getCoinFromMeta(const CSparkMintMeta& meta, const spark::IncomingViewKey& incomingViewKey);
CSparkMintMeta getMetadata(const spark::Coin& coin, const spark::IncomingViewKey& incoming_view_key);
spark::InputCoinData getInputData(spark::Coin coin, const spark::FullViewKey& full_view_key, const spark::IncomingViewKey& incoming_view_key);
spark::InputCoinData getInputData(std::pair<spark::Coin, CSparkMintMeta> coin, const spark::FullViewKey& full_view_key);

spark::IdentifiedCoinData identifyCoin(spark::Coin coin, const spark::IncomingViewKey& incoming_view_key);
#endif // SPARK_H