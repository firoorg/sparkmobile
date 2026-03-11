# SparkMobile

A C++17 cryptographic library implementing the [Spark](https://eprint.iacr.org/2021/1173) privacy protocol for the [Firo](https://firo.org/) cryptocurrency. Provides APIs for key management, address generation, minting, spending, and zero-knowledge proof systems. Designed for mobile integration.

## Features

- **Hierarchical key derivation**: SpendKey → FullViewKey → IncomingViewKey → Address
- **Zero-knowledge proofs**: Bulletproofs+ range proofs, Grootle one-of-many proofs, Chaum ownership proofs, Schnorr signatures
- **Transaction support**: Mint and spend transactions with full privacy
- **Address system**: Bech32-encoded addresses with diversifier support and F4Grumble obfuscation
- **Spark Names**: Human-readable alias system for Spark addresses
- **Authenticated encryption**: AES-256-GCM for coin data

## Prerequisites

- g++ with C++17 support
- OpenSSL (`libssl-dev`)
- Boost Unit Test Framework (`libboost-test-dev`)
- Autotools (`autoconf`, `automake`, `libtool`)
- pthreads

On Debian/Ubuntu:

```bash
sudo apt install build-essential autoconf automake libtool libssl-dev libboost-test-dev
```

## Building

Initialize the secp256k1 submodule if you haven't already:

```bash
git submodule update --init
```

Build all test binaries:

```bash
./build <output_dir>
# Example:
./build bin
```

The first run configures and compiles the secp256k1 submodule. Subsequent runs skip this step.

## Running Tests

Run all 16 test suites:

```bash
./run_all_tests <output_dir>
# Example:
./run_all_tests bin
```

Run individual tests:

```bash
./bin/spark_tests
./bin/full_test
./bin/spark_coin_tests
```

## Usage

### Key Generation

```cpp
#include "include/spark.h"

// Create keys
unsigned char keydata[32] = { /* 32 bytes of key material */ };
SpendKeyData skData(keydata);
spark::SpendKey spendKey = createSpendKey(skData);
spark::FullViewKey fullViewKey = createFullViewKey(spendKey);
spark::IncomingViewKey incomingViewKey = createIncomingViewKey(fullViewKey);

// Derive an address with a diversifier
spark::Address address = getAddress(incomingViewKey, 1);
```

### Minting Coins

```cpp
std::vector<spark::MintedCoinData> outputs = { /* recipients */ };
std::vector<unsigned char> serial_context = { /* context bytes */ };
std::vector<CRecipient> recipients = createSparkMintRecipients(outputs, serial_context, true);
```

### Identifying Received Coins

```cpp
spark::IdentifiedCoinData identified = identifyCoin(coin, incomingViewKey);
```

### Spending Coins

```cpp
createSparkSpendTransaction(
    spendKey, fullViewKey, incomingViewKey,
    recipients, privateRecipients,
    coins, cover_set_data, idAndBlockHashes,
    txHashSig, additionalTxSize,
    fee, serializedSpend, outputScripts, spentCoinsOut
);
```

## Project Structure

```
sparkmobile/
├── include/spark.h          # Public API header
├── src/                     # Core implementation
│   ├── spark.cpp            # High-level API
│   ├── keys.cpp             # Key management
│   ├── coin.cpp             # Coin structures
│   ├── mint_transaction.cpp # Minting
│   ├── spend_transaction.cpp# Spending
│   ├── bpplus.cpp           # Bulletproofs+ range proofs
│   ├── grootle.cpp          # One-of-many proofs
│   ├── chaum.cpp            # Ownership proofs
│   ├── schnorr.cpp          # Schnorr signatures
│   ├── aead.cpp             # AES-256-GCM encryption
│   ├── bech32.cpp           # Address encoding
│   ├── f4grumble.cpp        # Address obfuscation
│   └── ...
├── bitcoin/                 # Adapted Bitcoin Core utilities
├── secp256k1/               # libsecp256k1 submodule
├── tests/                   # Boost.Test unit tests
├── build                    # Build script
└── run_all_tests            # Test runner script
```

## License

[MIT](LICENSE) - Copyright (c) 2022 Firo
