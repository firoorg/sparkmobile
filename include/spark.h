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
struct CRecipient
{
    CScript pubKey;
    CAmount amount;
    bool subtractFeeFromAmount;
};

std::vector<CRecipient> CreateSparkMintRecipients(const std::vector<spark::MintedCoinData>& outputs, const std::vector<unsigned char>& serial_context, bool generate);
bool GetCoinsToSpend(CAmount required, std::vector<std::pair<spark::Coin, CSparkMintMeta>>& coinsToSpend_out, std::list<std::pair<spark::Coin, CSparkMintMeta>> coins,  int64_t& changeToMint, const size_t coinsToSpendLimit);
std::vector<std::pair<CAmount, std::vector<std::pair<spark::Coin, CSparkMintMeta>>>> SelectSparkCoins(CAmount required, bool subtractFeeFromAmount, std::list<std::pair<spark::Coin, CSparkMintMeta>> coins, std::size_t mintNum);

spark::Address getAddress(const std::vector<unsigned char> serialized_fullvKey, const uint64_t diversifier);

void getSparkSpendScripts(const std::vector<unsigned char>& serialized_fullvKey,
                          const std::vector<unsigned char>& serialized_spendKey,
                          const std::vector<spark::InputCoinData>& inputs,
                          const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data,
                          const std::map<uint64_t, uint256>& idAndBlockHashes,
                          CAmount fee,
                          uint64_t transparentOut,
                          const std::vector<spark::OutputCoinData>& privOutputs,
                          std::vector<uint8_t>& inputScript, std::vector<std::vector<unsigned char>>& outputScripts);

// CSparkMintMeta is defined at ../src/primitives.h
spark::Coin getCoinFromMeta(const CSparkMintMeta& meta, const std::vector<unsigned char>& serialized_fullvKey);
#endif // SPARK_H