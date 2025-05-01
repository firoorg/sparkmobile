# ATTENTION: before running this script, please follow the steps described in BUILD_OPENSSL.txt first

export CFLAGS="-I`pwd`/openssl/include $CFLAGS"
export CXXFLAGS="-I`pwd`/openssl/include $CXXFLAGS"

cd ../secp256k1 && ./autogen.sh
./configure --enable-experimental --enable-module-ecdh --with-bignum=no --enable-endomorphism   --with-asm=no CC=emcc CXX=em++ AR=emar RANLIB=emranlib --host=wasm32
make CC=emcc
cd -

emcc -std=c++17 -O3 -fexceptions -s WASM=1 -s USE_BOOST_HEADERS=1 -g \
     -I./openssl/include ./openssl/lib/libcrypto.a -I../secp256k1/include ../secp256k1/.libs/libsecp256k1.a \
     -s "EXPORTED_FUNCTIONS=['_js_createSpendKeyData', '_js_createSpendKey', '_js_getSpendKey_s1', '_js_getSpendKey_s2', '_js_getSpendKey_r',
                             '_js_getSpendKey_s1_hex', '_js_getSpendKey_s2_hex', '_js_getSpendKey_r_hex',
                             '_js_createFullViewKey', '_js_createIncomingViewKey', '_js_getAddress', '_js_encodeAddress', '_js_isValidSparkAddress',
                             '_js_decodeAddress', '_js_createMintedCoinData', '_js_createSparkMintRecipients',
                             '_js_getRecipientVectorLength', '_js_getRecipientAt', '_js_getRecipientScriptPubKey', '_js_getRecipientScriptPubKeySize',
                             '_js_getRecipientAmount', '_js_getRecipientSubtractFeeFromAmountFlag', '_js_deserializeCoin', '_js_getCoinFromMeta',
                             '_js_getMetadata', '_js_getInputData', '_js_getInputDataWithMeta', '_js_identifyCoin',
                             '_js_getIdentifiedCoinDiversifier', '_js_getIdentifiedCoinValue', '_js_getIdentifiedCoinMemo',
                             '_js_getCSparkMintMetaHeight', '_js_getCSparkMintMetaId', '_js_getCSparkMintMetaIsUsed', '_js_getCSparkMintMetaMemo',
                             '_js_getCSparkMintMetaDiversifier', '_js_getCSparkMintMetaValue', '_js_getCSparkMintMetaType', '_js_getCSparkMintMetaCoin',
                             '_js_getInputCoinDataCoverSetId', '_js_getInputCoinDataIndex', '_js_getInputCoinDataValue',
                             '_js_freeSpendKeyData', '_js_freeSpendKey', '_js_freeFullViewKey', '_js_freeIncomingViewKey',
                             '_js_freeAddress', '_js_freeRecipientVector', '_js_freeCSparkMintMeta', '_js_freeInputCoinData',
                             '_js_freeIdentifiedCoinData', '_js_freeCoin', '_malloc', '_free']" \
     -s "EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']" \
     -o spark.html ../src/*.cpp ../bitcoin/*.cpp ../bitcoin/support/*.cpp ../bitcoin/crypto/*.cpp
