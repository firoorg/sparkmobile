# CLAUDE.md - SparkMobile

## Project Overview

SparkMobile is a C++17 cryptographic library implementing the Spark privacy protocol for the Firo cryptocurrency. It provides APIs for key management, address generation, minting transactions, spending transactions, and zero-knowledge proof systems. Designed for mobile integration.

## Repository Structure

```
sparkmobile/
├── include/spark.h        # Public API header (main entry point)
├── src/                   # Core implementation (~20 .cpp/.h pairs)
│   ├── spark.cpp/h        # High-level API functions
│   ├── keys.cpp/h         # SpendKey, FullViewKey, IncomingViewKey
│   ├── coin.cpp/h         # Coin data structures
│   ├── mint_transaction.cpp/h   # Mint transaction creation/verification
│   ├── spend_transaction.cpp/h  # Spend transaction creation/verification
│   ├── params.cpp/h       # Protocol parameters
│   ├── primitives.cpp/h   # CSparkMintMeta, CoverSetData, etc.
│   ├── bpplus.cpp/h       # Bulletproofs+ range proofs
│   ├── grootle.cpp/h      # One-of-many proofs
│   ├── chaum.cpp/h        # Chaum ownership proofs
│   ├── schnorr.cpp/h      # Schnorr signature proofs
│   ├── aead.cpp/h         # Authenticated encryption (AES-256-GCM)
│   ├── bech32.cpp/h       # Address encoding
│   ├── f4grumble.cpp/h    # Address obfuscation
│   ├── transcript.cpp/h   # Fiat-Shamir transcript protocol
│   ├── hash.cpp/h         # Hash utilities
│   ├── kdf.cpp/h          # Key derivation
│   ├── sparkname.cpp/h    # Spark name alias system
│   └── util.cpp/h         # Utility functions
├── bitcoin/               # Bitcoin Core utilities (serialization, crypto, script)
│   ├── crypto/            # AES, SHA256, HMAC-SHA256/512
│   └── support/           # Memory cleanse, allocators
├── secp256k1/             # Vendored libsecp256k1 source (elliptic curve crypto)
├── tests/                 # Boost.Test unit tests (one per module)
├── build                  # Build script (bash)
├── run_all_tests          # Test runner script (bash)
└── LICENSE                # MIT
```

## Build & Test

### Prerequisites

- g++ with C++17 support
- OpenSSL (`libssl`, `libcrypto`)
- Boost Unit Test Framework (`libboost_unit_test_framework`)
- autotools (for secp256k1: `autoconf`, `automake`, `libtool`)
- pthreads

### Building

```bash
./build <output_dir>
# Example: ./build bin
```

The build configures the vendored secp256k1 library when its build files are missing, updates it with `make`, then compiles the shared Spark sources once and links all test binaries into `<output_dir>/`.

Set `BOOST_TEST_LIB` when the platform uses a suffixed Boost.Test library name, for example `BOOST_TEST_LIB=boost_unit_test_framework-mt` on MSYS2.

### Running Tests

```bash
./run_all_tests <output_dir>
# Example: ./run_all_tests bin
```

Runs all 16 test suites sequentially. Individual tests can also be run directly:

```bash
./<output_dir>/spark_tests
./<output_dir>/full_test          # Integration tests
./<output_dir>/spark_coin_tests
# etc.
```

### Compilation Pattern

The build compiles the shared sources into a static library, then links each test binary against it:

```
g++ tests/<test>.cpp <output_dir>/libsparkmobile.a \
  -g -Isecp256k1/include secp256k1/.libs/libsecp256k1.a \
  -lssl -lcrypto -lpthread -lboost_unit_test_framework -std=c++17 -o <dir>/<binary>
```

## Key Architecture Concepts

### Key Hierarchy

`SpendKey` -> `FullViewKey` -> `IncomingViewKey` -> `Address`

- **SpendKey**: 32 bytes of key data + index; required to spend coins
- **FullViewKey**: derived from SpendKey; can identify and decode all coin data
- **IncomingViewKey**: derived from FullViewKey; can identify incoming coins
- **Address**: derived from IncomingViewKey + diversifier; public receiving address

### Core Workflow

1. Generate keys: `createSpendKey()` -> `createFullViewKey()` -> `createIncomingViewKey()`
2. Get address: `getAddress(incomingViewKey, diversifier)`
3. Mint coins: `createSparkMintRecipients()`
4. Identify received coins: `identifyCoin()`, `getMetadata()`
5. Spend coins: `createSparkSpendTransaction()`

### Proof Systems

- **BPPlus**: Range proofs ensuring values are valid
- **Grootle**: One-of-many membership proofs (anonymity set)
- **Chaum**: Ownership proofs
- **Schnorr**: Signature proofs for authorization

### Namespaces

All Spark protocol types live in the `spark::` namespace. Bitcoin utility types (`CScript`, `uint256`, `CAmount`, etc.) are in the global namespace.

## Code Conventions

- **No formal linter or formatter** configured; follow existing style
- **Naming**: camelCase for functions and variables, PascalCase for classes/structs
- **Headers**: use `#ifndef`/`#define` include guards (not `#pragma once`)
- **Testing**: Boost.Test with `BOOST_AUTO_TEST_SUITE` / `BOOST_AUTO_TEST_CASE` macros
- **Commit messages**: short imperative descriptions, PR references with `(#N)` format

## Important Constants

Defined in `include/spark.h`:

- `SPARK_OUT_LIMIT_PER_TX`: 16 outputs per transaction
- `SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION`: 10000 COIN
- `OP_SPARKSPEND`: 0xd3 (spend opcode)
- `TRANSACTION_SPARK`: 9 (transaction type)
- `SPARK_CHANGE_D`: 0x270F (change address diversifier)

## Common Pitfalls

- secp256k1 is vendored in this repository and does not require submodule initialization
- The build script requires an output directory argument; it won't build without one
- Shared sources are rebuilt on each invocation; `make` updates secp256k1 when its sources change
- The `bitcoin/` directory contains adapted Bitcoin Core code, not a full Bitcoin dependency
