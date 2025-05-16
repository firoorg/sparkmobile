import spark from "/spark.js"
// Wait for the WebAssembly module to load
spark().then(
 function (Module) {
   console.log('WebAssembly Module Loaded');

  try {
   const keyData = new Uint8Array([
       0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
       0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
       0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
       0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
   ]); // 32 bytes total
   const index = 1; // Example index
   const diversifier = 4n; // Example diversifier
   const is_test_network = 0; // Working with the main network, therefore

   // Allocate memory in WASM for the 32-byte key data
   const keyDataPtr = Module._malloc(keyData.length);
   Module.HEAPU8.set(keyData, keyDataPtr);

   // Call the `js_createSpendKeyData` function to create SpendKeyData
   const spendKeyDataObj = Module.ccall(
       "js_createSpendKeyData",    // function name
       "number",                   // return type
       ["number", "number"],       // argument types
       [keyDataPtr, index]         // arguments
   );

   if (spendKeyDataObj === 0) {
       console.error("Failed to create SpendKeyData");
       Module._free(keyDataPtr);
       return;
   }

   // Call the `js_createSpendKey` function to create a SpendKey
   const spendKeyObj = Module.ccall(
       "js_createSpendKey",
       "number",                   // return type
       ["number"],                 // argument types
       [spendKeyDataObj]           // arguments
   );

   if (spendKeyObj === 0) {
       console.error("Failed to create SpendKey");
       Module.ccall("js_freeSpendKeyData", null, ["number"], [spendKeyDataObj]);
       Module._free(keyDataPtr);
       return;
   }

   // Get the `s1` scalar value as a string using `js_getSpendKey_s1`
   const s1 = Module.ccall(
       "js_getSpendKey_s1",        // function name
       "string",                   // return type
       ["number"],                 // argument types
       [spendKeyObj]               // arguments
   );
   console.log("SpendKey s1 Scalar:", s1);

   // Get the `s2` scalar value as a string using `js_getSpendKey_s2`
   const s2 = Module.ccall(
       "js_getSpendKey_s2",        // function name
       "string",                   // return type
       ["number"],                 // argument types
       [spendKeyObj]               // arguments
   );
   console.log("SpendKey s2 Scalar:", s2);

   // Get the `r` scalar value as a string using `js_getSpendKey_r`
   const r = Module.ccall(
       "js_getSpendKey_r",        // function name
       "string",                   // return type
       ["number"],                 // argument types
       [spendKeyObj]               // arguments
   );
   console.log("SpendKey r Scalar:", r);

   // Get the `s1` scalar value as a hex string using `js_getSpendKey_s1_hex`
   const s1_hex = Module.ccall(
       "js_getSpendKey_s1_hex",        // function name
       "string",                   // return type
       ["number"],                 // argument types
       [spendKeyObj]               // arguments
   );
   console.log("SpendKey s1 Scalar Hex:", s1_hex);

   // Get the `s2` scalar value as a hex string using `js_getSpendKey_s2_hex`
   const s2_hex = Module.ccall(
       "js_getSpendKey_s2_hex",        // function name
       "string",                   // return type
       ["number"],                 // argument types
       [spendKeyObj]               // arguments
   );
   console.log("SpendKey s2 Scalar Hex:", s2_hex);

   // Get the `r` scalar value as a hex string using `js_getSpendKey_r_hex`
   const r_hex = Module.ccall(
       "js_getSpendKey_r_hex",        // function name
       "string",                   // return type
       ["number"],                 // argument types
       [spendKeyObj]               // arguments
   );
   console.log("SpendKey r Scalar Hex:", r_hex);

   // Call the `js_createFullViewKey` function to create a FullViewKey
   const fullViewKeyObj = Module.ccall(
       "js_createFullViewKey",
       "number",                   // return type
       ["number"],                 // argument types
       [spendKeyObj]           // arguments
   );

   if (fullViewKeyObj === 0) {
       console.error("Failed to create FullViewKey");
       Module.ccall("js_freeSpendKey", null, ["number"], [spendKeyObj]);
       Module.ccall("js_freeSpendKeyData", null, ["number"], [spendKeyDataObj]);
       Module._free(keyDataPtr);
       return;
   }

   // Call the `js_createIncomingViewKey` function to create an IncomingViewKey
   const incomingViewKeyObj = Module.ccall(
       "js_createIncomingViewKey",
       "number",                   // return type
       ["number"],                 // argument types
       [fullViewKeyObj]           // arguments
   );

   if (incomingViewKeyObj === 0) {
       console.error("Failed to create IncomingViewKey");
       Module.ccall("js_freeFullViewKey", null, ["number"], [fullViewKeyObj]);
       Module.ccall("js_freeSpendKey", null, ["number"], [spendKeyObj]);
       Module.ccall("js_freeSpendKeyData", null, ["number"], [spendKeyDataObj]);
       Module._free(keyDataPtr);
       return;
   }

   // Call the `js_getAddress` function to create an Address
   const addressObj = Module.ccall(
       "js_getAddress",
       "number",                   // return type
       ["number", "number"],                 // argument types
       [incomingViewKeyObj, diversifier]           // arguments
   );

   if (addressObj === 0) {
       console.error("Failed to get Address");
       Module.ccall("js_freeIncomingViewKey", null, ["number"], [incomingViewKeyObj]);
       Module.ccall("js_freeFullViewKey", null, ["number"], [fullViewKeyObj]);
       Module.ccall("js_freeSpendKey", null, ["number"], [spendKeyObj]);
       Module.ccall("js_freeSpendKeyData", null, ["number"], [spendKeyDataObj]);
       Module._free(keyDataPtr);
       return;
   }

   // Encode the address as a string using `js_encodeAddress`
   const address_enc_main = Module.ccall(
       "js_encodeAddress",        // function name
       "string",                   // return type
       ["number", "number"],                 // argument types
       [addressObj, is_test_network]               // arguments
   );
   console.log("Address string (main):", address_enc_main);


       // Create MintedCoinData objects
       const outputsLength = 2; // Example: 2 outputs
       const outputsArray = [];

       for (let i = 0n; i < outputsLength; i++) {
           const mintedCoinData = Module.ccall(
              "js_createMintedCoinData",  // C++ function name
              "number",                   // Return type
              ["number", "number", "string"],  // Argument types
              [addressObj,
                 1000n + i * 100n, // Example amount
                 `Memo ${i}`]  // Example memo
           );
           if (!mintedCoinData) {
               throw new Error(`Failed to create MintedCoinData for index ${i}`);
           }
           outputsArray.push(mintedCoinData);
       }

       // Allocate memory for the array of pointers
       const pointerSize = 4; // Pointer = 4 bytes (32-bit) on most systems
       const outputsPointerArray = Module._malloc(outputsLength * pointerSize);

       // Write pointers to the allocated memory
       outputsArray.forEach((outputPointer, index) => {
           Module.HEAP32[(outputsPointerArray >> 2) + index] = outputPointer;
       });

       // Serial context (dummy data)
       const serialContext = new Uint8Array([0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC]);
       const serialContextPointer = Module._malloc(serialContext.length);
       Module.HEAPU8.set(serialContext, serialContextPointer); // Copy to Wasm memory

       // Call the `js_createSparkMintRecipients` function
       const recipientsVectorPtr = Module.ccall(
           "js_createSparkMintRecipients",       // C++ function name
           "number",                             // Return type
           ["number", "number", "number", "number", "number"], // Argument types
           [
               outputsPointerArray,              // Pointer array with minted coin data
               outputsLength,                    // Length of the outputs array
               serialContextPointer,             // Pointer to the serial context
               serialContext.length,             // Length of the serial context
               1                                 // Generate flag (true)
           ]
       );
       if (!recipientsVectorPtr) {
           throw new Error('Failed to call `js_createSparkMintRecipients`.');
       }

       // Process the recipient data
       const recipientsLength = Module.ccall(
           "js_getRecipientVectorLength",  // C++ function name
           "number",                       // Return type
           ["number"],                     // Argument types
           [recipientsVectorPtr]           // Arguments
       );
       console.log(`Number of Recipients: ${recipientsLength}`);

       for (let i = 0; i < recipientsLength; i++) {
           const recipientPtr = Module.ccall(
               "js_getRecipientAt",     // C++ function name
               "number",                // Return type
               ["number", "number"],    // Argument types
               [recipientsVectorPtr, i] // Arguments
           );

           // Access the recipient fields
           const scriptPubKeySize = Module.ccall(
               "js_getRecipientScriptPubKeySize",    // C++ function name
               "number",                             // Return type
               ["number"],                           // Argument types
               [recipientPtr]                        // Arguments
           );
           const scriptPubKeyPointer = Module.ccall(
               "js_getRecipientScriptPubKey",       // C++ function name
               "number",                            // Return type
               ["number"],                          // Argument types
               [recipientPtr]                       // Arguments
           );
           const scriptPubKey = new Uint8Array(scriptPubKeySize);
           for (let j = 0; j < scriptPubKeySize; j++) {
               scriptPubKey[j] = Module.HEAPU8[scriptPubKeyPointer + j];
           }
           const amount = Module.ccall(
               "js_getRecipientAmount",             // C++ function name
               "number",                            // Return type
               ["number"],                          // Argument types
               [recipientPtr]                       // Arguments
           );
           const subtractFeeFlag = Module.ccall(
               "js_getRecipientSubtractFeeFromAmountFlag", // C++ function name
               "number",                                   // Return type
               ["number"],                                 // Argument types
               [recipientPtr]                              // Arguments
           );

           // Log the received recipient
           console.log(`Recipient ${i}:`);
           console.log(`  ScriptPubKey: ${Array.from(scriptPubKey).map(b => b.toString(16).padStart(2, '0')).join('')}`);
           console.log(`  Amount: ${amount}`);
           console.log(`  Subtract Fee Flag: ${subtractFeeFlag}`);
       }

       // Free allocated memory
       Module._free(serialContextPointer);
       Module._free(outputsPointerArray);
       Module.ccall("js_freeRecipientVector", null, ["number"], [recipientsVectorPtr]);

       // Example usage of `js_deserializeCoin`
       const serializedCoin = new Uint8Array([0x00, 0x00, 0x50, 0x02, 0xf6, 0xbf, 0x32, 0xc0, 0x85, 0xf7, 0xd9, 0x6c, 0x85, 0x73, 0x1f, 0x0b, 0x64, 0x3e, 0xcf, 0x9f, 0xd8, 0x34, 0xd5, 0x09, 0xa1, 0x75, 0xf9, 0x90, 0xf6, 0xe9, 0xd1, 0x56, 0x56, 0x01, 0x00, 0x36, 0xb8, 0x34, 0x3a, 0x68, 0x22, 0xe0, 0x92, 0xc3, 0x2e, 0x9a, 0x38, 0xbf, 0x06, 0x21, 0x68, 0xd8, 0x92, 0x3a, 0xba, 0xf8, 0x50, 0x85, 0x1f, 0xed, 0xa4, 0xe2, 0x4c, 0x7b, 0x32, 0x48, 0xf9, 0x01, 0x00, 0xdb, 0xd8, 0xd3, 0xf5, 0x44, 0x1a, 0xf4, 0x1b, 0x35, 0xed, 0xd5, 0x50, 0xbd, 0xce, 0x6a, 0xe9, 0x08, 0xb3, 0x39, 0x57, 0x1b, 0x19, 0x81, 0x0e, 0xf7, 0x43, 0xc4, 0x4c, 0x62, 0x45, 0x39, 0xfd, 0x01, 0x00, 0x52, 0x8d, 0xa6, 0x79, 0x2d, 0x16, 0xf9, 0xae, 0xe7, 0xdb, 0x3a, 0x05, 0x86, 0xdf, 0x5b, 0x57, 0xef, 0x4d, 0x11, 0xce, 0x42, 0xcc, 0x90, 0x25, 0x65, 0x3f, 0xbb, 0x88, 0x4a, 0x2b, 0x0b, 0x6f, 0x30, 0xd7, 0xc0, 0xd9, 0x11, 0x80, 0x6c, 0xf9, 0x4a, 0xa0, 0x5e, 0xfc, 0xe3, 0x7b, 0x8a, 0x1a, 0xed, 0x39, 0xf1, 0x1f, 0x37, 0xca, 0x69, 0x11, 0x47, 0x6c, 0x2f, 0x42, 0x5c, 0xdc, 0x0a, 0x3f, 0xc3, 0x7b, 0xa2, 0x7b, 0xdb, 0x7a, 0x6f, 0x7e, 0xc4, 0xe4, 0x47, 0x82, 0x1e, 0xda, 0x8d, 0xfa, 0xd9, 0x84, 0x3f, 0x10, 0xe6, 0x54, 0x2d, 0xc7, 0x24, 0x07, 0xc8, 0x7b, 0x2b, 0x10, 0xcd, 0x48, 0x79, 0xaf, 0xd8, 0xb9, 0x20, 0xf0, 0x6b, 0xec, 0x3a, 0xfa, 0x26, 0x2a, 0xb5, 0x6d, 0xd5, 0xbe, 0x88, 0x02, 0xec, 0xd3, 0x63, 0xdd, 0x68, 0x00, 0xb5, 0xa2, 0x13, 0x70, 0x89, 0xe5, 0xe3, 0xe8, 0x36, 0x5f, 0xea, 0xca, 0x27, 0xa0, 0x39, 0xa0, 0x12, 0x00, 0x00, 0x00, 0x00, 0x65, 0x9a, 0xce, 0x99, 0xbb, 0x84, 0x04, 0x5f, 0xd4, 0x2b, 0x9b, 0xbe, 0xa3, 0x8c, 0x5e, 0x5f, 0xc2, 0x91, 0x39, 0xb8, 0x79, 0x1f, 0x54, 0x7a, 0xa2, 0xfe, 0xb4, 0xc2, 0x2e, 0x1f, 0x0c, 0xae, 0x29, 0x91, 0x1a, 0x5d, 0x68, 0x2b, 0x02, 0x7f, 0x2e, 0x19, 0x30, 0xef, 0x40, 0xf5, 0xd4, 0xeb, 0xd7, 0xdc, 0x3d, 0x59, 0xf9, 0x3b, 0x2c, 0xf6, 0xda, 0x59, 0xf8, 0xac, 0x01, 0x0c, 0xfd, 0x58, 0xe2, 0x01, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff]);
       const serializedCoinPointer = Module._malloc(serializedCoin.length);
       Module.HEAPU8.set(serializedCoin, serializedCoinPointer);
       const deserializedCoinObj = Module.ccall(
           "js_deserializeCoin",            // C++ function name
           "number",                        // Return type
           ["number", "number"],            // Argument types
           [serializedCoinPointer, serializedCoin.length] // Arguments
       );
       if (!deserializedCoinObj) {
           throw new Error('Failed to deserialize coin.');
       }
       console.log("Deserialized Coin:", deserializedCoinObj);

       // Example usage of `js_getMetadata`
       const metadataObj = Module.ccall(
           "js_getMetadata",               // C++ function name
           "number",                       // Return type
           ["number", "number"],           // Argument types
           [deserializedCoinObj, incomingViewKeyObj] // Arguments
       );
       if (!metadataObj) {
           console.log('Failed to get metadata from coin.');
       }
       else {
          console.log("Coin Metadata:", metadataObj);

          // Example usage of `js_getCoinFromMeta`
          const coinFromMetaObj = Module.ccall(
              "js_getCoinFromMeta",          // C++ function name
              "number",                      // Return type
              ["number", "number"],          // Argument types
              [metadataObj, incomingViewKeyObj] // Arguments
          );
          if (!coinFromMetaObj) {
              throw new Error('Failed to get coin from metadata.');
          }
          console.log("Coin from Metadata:", coinFromMetaObj);

          // Example usage of `js_getInputData`
          const inputDataObj = Module.ccall(
              "js_getInputData",                                     // C++ function name
              "number",                                             // Return type
              ["number", "number", "number"],                       // Argument types
              [deserializedCoinObj, fullViewKeyObj, incomingViewKeyObj] // Arguments
          );
          if (!inputDataObj) {
              throw new Error('Failed to get input data.');
          }
          console.log("Input Data:", inputDataObj);

          // Get Input Coin Data Cover Set ID
          const inputCoverSetId = Module.ccall(
              "js_getInputCoinDataCoverSetId", // C++ function name
              "number",                        // Return type
              ["number"],                      // Argument types
              [inputDataObj]                   // Arguments
          );
          console.log("Input Coin Data Cover Set ID:", inputCoverSetId);
          
          // Get Input Coin Data Index
          const inputIndex = Module.ccall(
              "js_getInputCoinDataIndex",     // C++ function name
              "number",                       // Return type
              ["number"],                     // Argument types
              [inputDataObj]                  // Arguments
          );
          console.log("Input Coin Data Index:", inputIndex);
          
          // Get Input Coin Data Value
          const inputValue = Module.ccall(
              "js_getInputCoinDataValue",     // C++ function name
              "number",                       // Return type
              ["number"],                     // Argument types
              [inputDataObj]                  // Arguments
          );
          console.log("Input Coin Data Value:", inputValue);
          
          // Get Input Data With Meta
          const inputDataWithMetaObj = Module.ccall(
              "js_getInputDataWithMeta",      // C++ function name
              "number",                       // Return type
              ["number", "number", "number"], // Argument types
              [deserializedCoinObj, metadataObj, fullViewKeyObj] // Arguments
          );
          if (!inputDataWithMetaObj) {
              throw new Error('Failed to get input data with metadata.');
          }
          console.log("Input Data with Metadata:", inputDataWithMetaObj);

          // Free allocated memory
          Module.ccall("js_freeInputCoinData", null, ["number"], [inputDataWithMetaObj]);
          Module.ccall("js_freeInputCoinData", null, ["number"], [inputDataObj]);
       }

       // Example usage of `js_identifyCoin`
       const identifiedCoinObj = Module.ccall(
           "js_identifyCoin",             // C++ function name
           "number",                      // Return type
           ["number", "number"],          // Argument types
           [deserializedCoinObj, incomingViewKeyObj] // Arguments
       );
       if (!identifiedCoinObj) {
           console.log('Failed to identify coin.');
       }
       else {
          console.log("Identified Coin:", identifiedCoinObj);

          // Get Identified Coin Diversifier
          const coinDiversifier = Module.ccall(
              "js_getIdentifiedCoinDiversifier", // C++ function name
              "number",                          // Return type
              ["number"],                        // Argument types
              [identifiedCoinObj]                // Arguments
          );
          console.log("Identified Coin Diversifier:", coinDiversifier);
          
          // Get Identified Coin Value
          const value = Module.ccall(
              "js_getIdentifiedCoinValue",      // C++ function name
              "number",                         // Return type
              ["number"],                       // Argument types
              [identifiedCoinObj]               // Arguments
          );
          console.log("Identified Coin Value:", value);
          
          // Get Identified Coin Memo
          const memo = Module.ccall(
              "js_getIdentifiedCoinMemo",       // C++ function name
              "string",                         // Return type
              ["number"],                       // Argument types
              [identifiedCoinObj]               // Arguments
          );
          console.log("Identified Coin Memo:", memo);
       }

       // Get Spark Mint Meta Height
       const metaHeight = Module.ccall(
           "js_getCSparkMintMetaHeight",  // C++ function name
           "number",                      // Return type
           ["number"],                    // Argument types
           [metadataObj]                  // Arguments
       );
       console.log("Spark Mint Meta Height:", metaHeight);
       
       // Get Spark Mint Meta ID
       const metaId = Module.ccall(
           "js_getCSparkMintMetaId",     // C++ function name
           "number",                     // Return type
           ["number"],                   // Argument types
           [metadataObj]                 // Arguments
       );
       console.log("Spark Mint Meta ID:", metaId);

       // Get Spark Mint Meta Is Used
       const metaIsUsed = Module.ccall(
           "js_getCSparkMintMetaIsUsed", // C++ function name
           "number",                     // Return type
           ["number"],                   // Argument types
           [metadataObj]                 // Arguments
       );
       console.log("Spark Mint Meta Is Used:", metaIsUsed !== 0);

       // Get Spark Mint Meta Memo
       const metaMemo = Module.ccall(
           "js_getCSparkMintMetaMemo",   // C++ function name
           "string",                     // Return type
           ["number"],                   // Argument types
           [metadataObj]                 // Arguments
       );
       console.log("Spark Mint Meta Memo:", metaMemo);

       // Get Spark Mint Meta Diversifier
       const metaDiversifier = Module.ccall(
           "js_getCSparkMintMetaDiversifier", // C++ function name
           "number",                          // Return type
           ["number"],                        // Argument types
           [metadataObj]                      // Arguments
       );
       console.log("Spark Mint Meta Diversifier:", metaDiversifier);

       // Get Spark Mint Meta Value
       const metaValue = Module.ccall(
           "js_getCSparkMintMetaValue",  // C++ function name
           "number",                     // Return type
           ["number"],                   // Argument types
           [metadataObj]                 // Arguments
       );
       console.log("Spark Mint Meta Value:", metaValue);

       // Get Spark Mint Meta Type
       const metaType = Module.ccall(
           "js_getCSparkMintMetaType",   // C++ function name
           "number",                     // Return type
           ["number"],                   // Argument types
           [metadataObj]                 // Arguments
       );
       console.log("Spark Mint Meta Type:", metaType);
       
       // Get Spark Mint Meta Coin
       const metaCoinObj = Module.ccall(
           "js_getCSparkMintMetaCoin",   // C++ function name
           "number",                     // Return type
           ["number"],                   // Argument types
           [metadataObj]                 // Arguments
       );
       console.log("Spark Mint Meta Coin:", metaCoinObj);

       // Free allocated memory
       Module.ccall("js_freeCoin", null, ["number"], [deserializedCoinObj]);
       Module.ccall("js_freeIdentifiedCoinData", null, ["number"], [identifiedCoinObj]);
       Module.ccall("js_freeCSparkMintMeta", null, ["number"], [metadataObj]);
       Module.ccall("js_freeAddress", null, ["number"], [addressObj]);
       Module.ccall("js_freeIncomingViewKey", null, ["number"], [incomingViewKeyObj]);
       Module.ccall("js_freeFullViewKey", null, ["number"], [fullViewKeyObj]);
       Module.ccall("js_freeSpendKey", null, ["number"], [spendKeyObj]);
       Module.ccall("js_freeSpendKeyData", null, ["number"], [spendKeyDataObj]);
       Module._free(keyDataPtr);

   } catch (error) {
       console.error('Error:', error);
   }
} )
