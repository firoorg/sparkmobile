#ifndef SPARK_H
#define SPARK_H

#include "../bitcoin/script.h"
#include "../bitcoin/amount.h"
#include "../src/primitives.h"
#include "../src/coin.h"
#include "../src/mint_transaction.h"

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

// CSparkMintMeta is defined at ../src/primitives.h
spark::Coin getCoinFromMeta(const CSparkMintMeta& meta);
#endif // SPARK_H