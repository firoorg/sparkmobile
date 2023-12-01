#include "../include/spark.h"
#include "utils.h"

#include <cstring>
#include "../bitcoin/uint256.h"
#include <iostream> // Just for printing.

#ifndef EXPORT_DART
    #ifdef __cplusplus
        #define EXPORT_DART extern "C" __attribute__((visibility("default"))) __attribute__((used))
    #else
        #define EXPORT_DART __attribute__((visibility("default"))) __attribute__((used))
    #endif
    #ifdef _WIN32
        #define EXPORT_DART __declspec(dllexport)
    #endif
#endif

using namespace spark;

/*
 * FFI-friendly wrapper for spark::getAddress.
 *
 * Uses the utility function spark::createSpendKeyFromData(const char *keyData, int index) to pass
 * parameters to the C++ function spark::getAddress(const SpendKey& spendKey, uint64_t diversifier),
 * then uses the utility function std::string encode(const uint8_t network) to convert the result
 * back to a C string.
 *
 * getAddress: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/spark.cpp#L388
 */
EXPORT_DART
const char* getAddress(const char* keyDataHex, int index, int diversifier, int isTestNet) {
    try {
        // Use the hex string directly to create the SpendKey.
        spark::SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);

        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);
        spark::Address address(incomingViewKey, static_cast<uint64_t>(diversifier));

        // Encode the Address object into a string using the appropriate network.
        std::string encodedAddress = address.encode(isTestNet ? spark::ADDRESS_NETWORK_TESTNET : spark::ADDRESS_NETWORK_MAINNET);

        // Allocate memory for the C-style string.
        char* cstr = new char[encodedAddress.length() + 1];
        std::strcpy(cstr, encodedAddress.c_str());

        return cstr;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    }
}

/*
 * FFI-friendly wrapper for spark:identifyCoin.
 *
 * Uses the utility functions spark::Coin fromFFI(const CCoin& c_struct) to pass parameters to the
 * C++ function spark::identifyCoin(const Coin& coin), then uses the utility function
 * CIdentifiedCoinData toFFI(const spark::IdentifiedCoinData& cpp_struct) to convert the result back
 * to a C struct.
 *
 * We also need the incoming view key or CCoin we need to derive it, so accept keyDataHex and index.
 *
 * identifyCoin: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/spark.cpp#L400
 */
EXPORT_DART
struct CIdentifiedCoinData identifyCoin(struct CCoin c_struct, const char* keyDataHex, int index) {
    try {
        spark::Coin coin = fromFFI(c_struct);

        // Derive the incoming view key from the key data and index.
        spark::SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);
        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);

        spark::IdentifiedCoinData identifiedCoinData = coin.identify(incomingViewKey);
        return toFFI(identifiedCoinData);
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return CIdentifiedCoinData();
    }
}

/*
 * FFI-friendly wrapper for spark::createSparkMintRecipients.
 *
 * Uses the utility functions spark::MintedCoinData fromFFI(const CMintedCoinData& c_struct) to pass
 * parameters to the C++ function spark::createSparkMintRecipients, then uses the utility function
 * CRecipient toFFI(const spark::Recipient& cpp_struct) to convert the result back to a C struct.
 *
 * createSparkMintRecipients: https://github.com/firoorg/sparkmobile/blob/8bf17cd3deba6c3b0d10e89282e02936d7e71cdd/src/spark.cpp#L43
 */
EXPORT_DART
struct CCRecipient* createSparkMintRecipients(
    struct CMintedCoinData* cOutputs,
    int outputsLength,
    const char* serial_context,
    int serial_contextLength,
    int generate
) {
    // Construct vector of spark::MintedCoinData.
    std::vector<spark::MintedCoinData> outputs;

    // Copy the data from the array to the vector.
    for (int i = 0; i < outputsLength; i++) {
        outputs.push_back(fromFFI(cOutputs[i]));
    }

    // Construct vector of unsigned chars.
    std::vector<unsigned char> blankSerialContext;

    // Copy the data from the array to the vector.
    for (int i = 0; i < serial_contextLength; i++) {
        blankSerialContext.push_back(serial_context[i]);
    }

    // Call spark::createSparkMintRecipients.
    std::vector<CRecipient> recipients = createSparkMintRecipients(outputs, blankSerialContext, generate);

    // Create a CRecipient* array.
    CCRecipient* cRecipients = new CCRecipient[recipients.size()];

    // Copy the data from the vector to the array.
    for (int i = 0; i < recipients.size(); i++) {
        cRecipients[i] = toFFI(recipients[i]);
    }

    // Return the array.
    return cRecipients;
}

/*
 * FFI-friendly wrapper for spark::createSparkSpendTransaction.
 *
 * createSparkSpendTransaction is the longest and most complicated function to be wrapped.  It
 * accepts several sparkmobile-specific structs which have been wrapped here and in utils.cpp.
 * These structs are then converted to the corresponding C++ structs and passed to the C++ function
 * spark::createSparkSpendTransaction.  The return signature is void and the result is written to
 * the serializedSpend parameter.
 *
 * Instead of accepting spend, full view, and incoming view keys, we accept the key data and index
 * and derive the keys here.  We could alternatively serialize and save keys to memory or disk and
 * deserialize them as needed, which would enable watch-only wallets, but this is simpler for now.
 * The rest of the parameters are relatively straightforward to convert from their C, FFI-friendly
 * structs to their C++ counterparts, with the exception of idAndBlockHashes_all, which doesn't
 * appear to be used in sparkmobile's createSparkSpendTransaction, so we ignore it and pass an empty
 * vector.
 *
 * The serializedSpend parameter is where the result is written.  We allocate memory for it here,
 * then copy the data from the vector to an array which is returned.  The caller is responsible for
 * freeing the memory.
 *
 * The original function is here:
 *
 * void createSparkSpendTransaction(
 * const spark::SpendKey& spendKey,
 * const spark::FullViewKey& fullViewKey,
 * const spark::IncomingViewKey& incomingViewKey,
 * const std::vector<std::pair<CAmount, bool>>& recipients,
 * const std::vector<std::pair<spark::OutputCoinData, bool>>& privateRecipients,
 * std::list<CSparkMintMeta> coins,
 * const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data_all,
 * const std::map<uint64_t, uint256>& idAndBlockHashes_all,
 * const uint256& txHashSig,
 * CAmount &fee,
 * std::vector<uint8_t>& serializedSpend,
 * std::vector<std::vector<unsigned char>>& outputScripts)
 *
 * The SpendKey, FullViewKey, and IncomingViewKey are replaced with the key data and index.
 * CAmounts are trivially-constructable from uint64_t.  We have wrapped OutputCoinData,
 * CSparkMintMeta, and CoverSetData as COuputCoinData, CCSparkMintMeta, and CCoverSetData,
 * respectively.  idAndBlockHashes_all is not used in sparkmobile's createSparkSpendTransaction,
 * so we ignore it and pass an empty vector.  The rest of the parameters are easy to convert.
 *
 * createSparkSpendTransaction: https://github.com/firoorg/sparkmobile/blob/23099b0d9010a970ad75b9cfe05d568d634088f3/src/spark.cpp#L190
 */
EXPORT_DART
unsigned char* cCreateSparkSpendTransaction(
    const char* keyDataHex,
    int index,
    struct CRecip* recipients, // This CRecip(ient) is not the same as a CRecipient.
    int recipientsLength,
    struct COutputRecipient* privateRecipients,
    int privateRecipientsLength,
    struct CCSparkMintMeta* coins,
    int coinsLength,
    struct CCoverSets* cover_set_data_all,
    int cover_set_data_allLength,
    // idAndBlockHashes_all is unused in spark::createSparkSpendTransaction so we ignore it here.
    const char* txHashSig, // A hex string.
    int txHashSigLength,
    uint64_t fee,
    // spark's createSparkSpendTransaction returns void, writing to the serializedSpend parameter.
    // We return it instead.
    const OutputScript* outputScripts,
    int outputScriptsLength
) {
    try {
        // Derive the keys from the key data and index.
        spark::SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);
        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);

        // Convert CRecipient* recipients to std::vector<std::pair<CAmount, bool>> cppRecipients.
        std::vector<std::pair<CAmount, bool>> cppRecipients;
        for (int i = 0; i < recipientsLength; i++) {
            cppRecipients.push_back(std::make_pair(recipients[i].amount, recipients[i].subtractFee));
        }

        // Convert COutputRecipient* privateRecipients to std::vector<std::pair<spark::OutputCoinData, bool>> cppPrivateRecipients.
        std::vector<std::pair<spark::OutputCoinData, bool>> cppPrivateRecipients;
        for (int i = 0; i < privateRecipientsLength; i++) {
            cppPrivateRecipients.push_back(std::make_pair(fromFFI(privateRecipients[i].output), privateRecipients[i].subtractFee));
        }

        // Convert CCSparkMintMeta* coins to std::list<CSparkMintMeta> cppCoins.
        std::list<CSparkMintMeta> cppCoins;
        for (int i = 0; i < coinsLength; i++) {
            cppCoins.push_back(fromFFI(coins[i]));
        }

        // Convert CCoverSets* cover_set_data_all to a std::unordered_map<uint64_t, spark::CoverSetData> cppCoverSetDataAll
        // TODO verify correctness.
        std::unordered_map<uint64_t, spark::CoverSetData> cppCoverSetDataAll;
        for (int i = 0; i < cover_set_data_allLength; i++) {
            for (int j = 0; j < cover_set_data_all[i].cover_setsLength; j++) {
                // Convert CCoverSetData to vector of Coins.
                std::vector<spark::Coin> cppCoverSetCoins;
                spark::Coin coin = fromFFI(*cover_set_data_all[i].cover_sets[j].cover_set[0]);
                cppCoverSetCoins.push_back(coin);

                // Convert CCoverSetData to vector of vector of unsigned chars.
                std::vector<CCoverSetData> coverSets(cover_set_data_all[i].cover_sets, cover_set_data_all[i].cover_sets + cover_set_data_all[i].cover_setsLength);

                // Combine all the cover set representations into one vector.
                std::vector<unsigned char> coverSetReps;
                for (int k = 0; k < cover_set_data_all[i].cover_setsLength; k++) {
                    for (int l = 0; l < cover_set_data_all[i].cover_sets[k].cover_set_representationLength; l++) {
                        coverSetReps.push_back(cover_set_data_all[i].cover_sets[k].cover_set_representation[l]);
                    }
                }

                // Construct spark::CoverSetData.
                spark::CoverSetData cppCoverSetData;
                cppCoverSetData.cover_set = cppCoverSetCoins;
                cppCoverSetData.cover_set_representation = coverSetReps;

                cppCoverSetDataAll[cover_set_data_all[i].cover_sets[j].cover_setLength] = cppCoverSetData;
            }
        }

        // Convert const char * txHashSig to a uint256 cppTxHashSig.
        // uint256 cppTxHashSig(txHashSig); // This throws `error: no matching function for call to ‘uint256::uint256(const char*&)’`, we need to:
        uint256 cppTxHashSig;
        cppTxHashSig.SetHex(txHashSig);

        // Make a CAmount cppFee from fee.
        CAmount cppFee = fee;

        // Make a dummy for idAndBlockHashes_all.
        std::map<uint64_t, uint256> cppIdAndBlockHashesAll;

        // Make a placeholder for serializedSpend.
        std::vector<uint8_t> cppSerializedSpend;

        // Convert outputScripts to std::vector<std::vector<unsigned char>> cppOutputScripts.
        std::vector<std::vector<unsigned char>> cppOutputScripts;
        for (int i = 0; i < outputScriptsLength; i++) {
            for (int j = 0; j < outputScripts[i].length; j++) {
                cppOutputScripts[i].push_back(outputScripts[i].bytes[j]);
            }
        }

        // Call spark::createSparkSpendTransaction.
        createSparkSpendTransaction(
            spendKey,
            fullViewKey,
            incomingViewKey,
            cppRecipients,
            cppPrivateRecipients,
            cppCoins,
            cppCoverSetDataAll,
            cppIdAndBlockHashesAll,
            cppTxHashSig,
            cppFee,
            cppSerializedSpend,
            cppOutputScripts
        );

        // Allocate memory for the C-style string.
        unsigned char* cstr = new unsigned char[cppSerializedSpend.size()];

        // Copy the data from the vector to the array.
        for (int i = 0; i < cppSerializedSpend.size(); i++) {
            cstr[i] = cppSerializedSpend[i];
        }

        // Return the array.
        return cstr;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    }
}
