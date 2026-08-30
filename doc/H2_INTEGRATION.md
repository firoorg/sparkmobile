# H2 integration: sparkmobile → flutter_libsparkmobile → Stack Wallet

Branch: `codex/spark-v2-restoration` (or `main` after merge).

Mainnet heights (use chain tip + 1 for spend construction):

| Height | Rule |
|--------|------|
| 1,371,000 | Use Chaum V2 (`nType` 11, `spendVersion` 2) |

## sparkmobile

Constants in `include/spark.h`:

- `SPARK_TX_VERSION` = 3
- `TRANSACTION_SPARK` = 9
- `TRANSACTION_SPARK_V2` = 11

Breaking API (vs old `main`):

```cpp
createSparkSpendTransaction(..., txHashSig, additionalTxSize,
    spark::SpendTransactionVersion version,
    const uint256& extensionCommitment,
    fee, serializedSpend, outputScripts, spentCoinsOut);

GetSparkNameScript(sparkNameData, ownershipDigest, version,
    spendKey, incomingViewKey, outputScript);
```

New helpers: `getSparkNameCommitment`, `getSparkNameOwnershipMessage`.

`SelectSparkCoins(..., version)` is exported from `include/spark.h` and adds V2 size to fee estimates (+32 bytes, +98 per extra input). FFI wrappers should call it instead of duplicating fee math.

Cover-set data: pass RPC `setHash` and `blockHash` keyed by `coin.groupId`. No library change for group rollover — use RPC values as-is.

## flutter_libsparkmobile

1. Update `src/deps/sparkmobile` to the H2 commit.
2. Extend FFI:
   - `cCreateSparkSpendTransaction`: `int spendVersion` (1 or 2), `unsigned char extensionCommitment[32]`
   - `estimateSparkFee`: `int spendVersion`
   - `createSparkNameScript`: replace `scalarMHex` with `ownershipDigest[32]` + `int spendVersion`
   - Optional: `getSparkNameCommitment`
3. Regenerate ffigen; update `flutter_libsparkmobile.dart`.

## Stack Wallet

File: `lib/wallets/wallet/wallet_mixin_interfaces/spark_interface.dart`

**1. Transaction type** — replace hard-coded V1:

```dart
// today: txb.setVersion(3 | (9 << 16));
const sparkTxVersion = 3;
const transactionSparkV2 = 11;
final useChaumV2 = nextBlockHeight >= 1371000; // mainnet
final nType = useChaumV2 ? transactionSparkV2 : 9;
txb.setVersion(sparkTxVersion | (nType << 16));
```

**2. Plugin spend** — extend `createSparkSendTransaction`:

- `spendVersion`: 2 when `useChaumV2`, else 1
- `extensionCommitment`: 32 zero bytes (plain spend)

**3. Fee** — pass `spendVersion: 2` into `estimateSparkFee` when `useChaumV2`.

**4. Spark Name** (V2 only):

- `extensionCommitment` = `getSparkNameCommitment(nameData)` on the spend
- `createSparkNameScript(..., ownershipDigest: txHash, spendVersion: 2)` for the final proof

**5. Unchanged**

- RPC fetch of `setHash` / `blockHash` by `coin.groupId`
- Single-input enforcement (`usedCoins.length == 1`)
- `txHash` = incomplete tx hash before Spark payload (already implemented)

## Ship order

`sparkmobile` → `flutter_libsparkmobile` → Stack Wallet (bump plugin ref + Dart changes above).
