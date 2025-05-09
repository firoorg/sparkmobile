#ifndef FIRO_LIBSPARK_SPARK_H
#define FIRO_LIBSPARK_SPARK_H
#include "../bitcoin/script.h"
#include "../bitcoin/amount.h"
#include "../src/primitives.h"
#include "../src/coin.h"
#include "../src/mint_transaction.h"
#include "../src/spend_transaction.h"
#include <list>

std::pair<CAmount, std::vector<CSparkMintMeta>> SelectSparkCoins(CAmount required, bool subtractFeeFromAmount, std::list<CSparkMintMeta> coins, std::size_t mintNum, std::size_t utxoNum, std::size_t additionalTxSize);

bool GetCoinsToSpend(
        CAmount required,
        std::vector<CSparkMintMeta>& coinsToSpend_out,
        std::list<CSparkMintMeta> coins,
        int64_t& changeToMint);


void getSparkSpendScripts(const spark::FullViewKey& fullViewKey,
                          const spark::SpendKey& spendKey,
                          const std::vector<spark::InputCoinData>& inputs,
                          const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data,
                          const std::map<uint64_t, uint256>& idAndBlockHashes,
                          CAmount fee,
                          uint64_t transparentOut,
                          const std::vector<spark::OutputCoinData>& privOutputs,
                          std::vector<uint8_t>& inputScript, std::vector<std::vector<unsigned char>>& outputScripts);


void ParseSparkMintTransaction(const std::vector<CScript>& scripts, spark::MintTransaction& mintTransaction);
void ParseSparkMintCoin(const CScript& script, spark::Coin& txCoin);

#endif //FIRO_LIBSPARK_SPARK_H
