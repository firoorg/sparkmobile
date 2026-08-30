# Spark wallet integration guide

End-to-end reference for integrating Spark into a mobile wallet using **sparkmobile**, a backend node or ElectrumX, and (optionally) **flutter_libsparkmobile**.

For H2 / Chaum V2 changes (mainnet height 1,371,000), see [H2_INTEGRATION.md](H2_INTEGRATION.md).

---

## Coin types

After Spark activation there are two coin types:

| Type | Description |
|------|-------------|
| **Transparent coin** | Bitcoin-style UTXO on the transparent layer |
| **Spark coin (mint)** | Private Spark coin held inside a Spark address |

---

## Address types

| Name | Layer | Example |
|------|-------|---------|
| **T-address** | Transparent (Bitcoin-like) | `a6Mdo9rz35GFUdsXyXXNj8KL7MAwyXSiXT` |
| **P-address** | Spark (private) | `pr18qqntc8e60x0ygnv0ey7skekve73tmvhhlkaehka6qfv56zc4w2j75jldrf8wjf8dy0hu33vsww7fj34fd3k7rnwgv7jdvtmgv2g37xqmm59krmgycgkdes37jqupc62s7khafqynlxsy` |

A **P-address** is encoded as a string, like a T-address. The user gives it to someone who should send Spark coins to them.

In **firo-qt** there is a “Receiving addresses” address book for T-addresses. Spark wallets need an equivalent for P-addresses.

**CLI address RPCs** (no arguments; return string or list):

- `getsparkdefaultaddress`
- `getallsparkaddresses`
- `getnewsparkaddress`

---

## Transfer types

After Spark activation (and the Lelantus graceful period ends), these transfer paths exist:

| # | From | To | Transaction kind |
|---|------|-----|------------------|
| 1 | UTXO | T-address | Regular transparent tx — Spark does not touch it |
| 2 | UTXO | P-address | **Mint** — transparent UTXO → Spark coin |
| 3 | Spark coin | T-address | **Spend** — change minted to default P-address |
| 4 | Spark coin | P-address | **Spend** — change minted to a P-address |

Combined transparent + Spark outputs in one spend (types 3 + 4 mixed) are supported at the protocol level; the wallet UI decides whether to expose that.

---

## Transaction types (summary)

1. **Regular (transparent)** — UTXO → UTXO. Standard Bitcoin transaction.
2. **Mint** — UTXO → Spark coin. Wallet builds a normal tx; outputs are Spark mint scripts instead of transparent outputs.
3. **Spark spend** — Spark coin → T-address and/or P-address. Proofs and payload are produced by sparkmobile.

---

## Node RPC reference

Full Spark functionality is available via **firo-cli** or the firo-qt debug console.

| RPC | Purpose |
|-----|---------|
| `listunspentsparkmints` | Unspent Spark coins. Each entry: `{txid, nHeight, scriptPubKey, amount}` |
| `listsparkmints` | All Spark coins. Each entry: `{txid, nHeight, nId, isUsed, lTagHash, scriptPubKey, amount}` |
| `listsparkspends` | Spark spends. Each entry: `{txid, lTagHash, lTag, amount}` |
| `getsparkdefaultaddress` | Default P-address (string) |
| `getnewsparkaddress` | New P-address (string) |
| `getallsparkaddresses` | All P-addresses. Each entry: `{diversifier, address}` |
| `getsparkbalance` | `{availableBalance, unconfirmedBalance, fullBalance}` |
| `getsparkaddressbalance` | Balance for one P-address (pass address string) |
| `resetsparkmints` | **Testing only** — marks all mints unused/unconfirmed; requires rescan |
| `setsparkmintstatus` | **Testing only** — `"lTagHash"`, `<isused>` (bool) |
| `mintspark` | Create mint(s). Args: `{"<P-address>": [amount, memo], ...}` → `txid` |
| `spendspark` | Create spend(s). Args: transparent map + private map → `txid`(s) |
| `lelantustospark` | Convert Lelantus → Spark during graceful period |

### `mintspark` examples

Send to two P-addresses:

```
mintspark "{\"pr18qqntc8e60x0ygnv0ey7skekve73tmvhhlkaehka6qfv56zc4w2j75jldrf8wjf8dy0hu33vsww7fj34fd3k7rnwgv7jdvtmgv2g37xqmm59krmgycgkdes37jqupc62s7khafqynlxsy\":[0.01, \"\"],\"pr1t0l6vu9h9a8nr203tfcesps46agtm0aa9uzsty0tp4wqqrg42rg35yf4r839t3fenlfmsgkpwwklxg5r68tvenn5uy29wwykany3t0qrkjy2res6thzwx90nha6wpkegwrm0n8g2cjawq\":[0.01, \"\"]}"
```

With memo on the second recipient, replace the second memo string with `"memo"`.

Reference implementation: [rpcwallet.cpp — mintspark](https://github.com/firoorg/firo/blob/master/src/wallet/rpcwallet.cpp) (search for `mintspark`).

### `spendspark` examples

Transparent only (change → default P-address):

```
spendspark "{\"TR1FW48J6ozpRu25U8giSDdTrdXXUYau7U\":[0.02, false]}" "{}"
```

Transparent + two private recipients:

```
spendspark "{\"TR1FW48J6ozpRu25U8giSDdTrdXXUYau7U\":[0.02, false]}" "{\"pr18qqntc8e60x0ygnv0ey7skekve73tmvhhlkaehka6qfv56zc4w2j75jldrf8wjf8dy0hu33vsww7fj34fd3k7rnwgv7jdvtmgv2g37xqmm59krmgycgkdes37jqupc62s7khafqynlxsy\":[0.01, \"\", false],\"pr1t0l6vu9h9a8nr203tfcesps46agtm0aa9uzsty0tp4wqqrg42rg35yf4r839t3fenlfmsgkpwwklxg5r68tvenn5uy29wwykany3t0qrkjy2res6thzwx90nha6wpkegwrm0n8g2cjawq\":[0.01, \"\", false]}"
```

Argument shape:

- **Transparent** — `{ "T-address": [amount, subtractFeeFromAmount] }`
- **Private** — `{ "P-address": [amount, memo, subtractFeeFromAmount] }` (memo may be empty)

Reference: [rpcwallet.cpp — spendspark](https://github.com/firoorg/firo/blob/master/src/wallet/rpcwallet.cpp) (search for `spendspark`).

### Testing-only RPCs

`resetsparkmints` and `setsparkmintstatus` are for manual testing or test harnesses, not production wallets.

### `lelantustospark`

Spends all Lelantus mints to transparent outputs and mints the result into the default P-address. Users can do the same manually (spend Lelantus, then mint Spark). **Must be done during the Lelantus graceful period.**

---

## Spark activation heights (Firo node)

| Network | Spark start block |
|---------|-------------------|
| Mainnet | 819,300 |
| Testnet | 107,000 |
| Devnet | 1,500 |
| Regtest | 100 |

(H2 Chaum V2 and related rules are documented in [H2_INTEGRATION.md](H2_INTEGRATION.md).)

---

## sparkmobile library

C++ library for mobile Spark crypto. Public API: [`include/spark.h`](../include/spark.h).

### Key hierarchy

```
SpendKey → FullViewKey → IncomingViewKey → Address (P-address)
```

| Function | Role |
|----------|------|
| `createSpendKey(SpendKeyData)` | Spending authority; derive on demand only |
| `createFullViewKey` / `createIncomingViewKey` | View keys for sync and identification |
| `getAddress(incomingViewKey, diversifier)` | Encode a P-address |

### Coin helpers

| Function | Role |
|----------|------|
| `getCoinFromMeta` | Rebuild `spark::Coin` from stored metadata |
| `getMetadata` | Build `CSparkMintMeta` from a coin + view key |
| `getInputData` | Input data for spends |
| `identifyCoin` | Returns identified data if coin belongs to wallet; throws otherwise |

### Mint (transparent → Spark)

| Function | Role |
|----------|------|
| `createSparkMintRecipients(outputs, serial_context, generate)` | Spark mint scripts and `CRecipient` outputs |

`spark::MintedCoinData` fields: `Address`, amount `v`, `memo` (max length = protocol memo size, 31 bytes by default).

`serial_context` must be unique per transaction — typically a serialization of all transparent inputs.

Build the transparent part of the tx first, then attach Spark mint outputs. Sign like a normal transaction.

### Spend (Spark → transparent / Spark)

| Function | Role |
|----------|------|
| `SelectSparkCoins(..., version)` | Coin selection + fee estimate for V1 or V2 |
| `createSparkSpendTransaction(...)` | Full spend payload and output scripts |

**`createSparkSpendTransaction` inputs:**

| Parameter | Meaning |
|-----------|---------|
| `recipients` | Transparent outputs: `(amount, subtractFeeFromAmount)` |
| `privateRecipients` | Spark outputs: `(OutputCoinData, subtractFeeFromAmount)` |
| `coins` | Wallet’s available Spark coins |
| `cover_set_data_all` | Anonymity sets keyed by group id (`nId`) |
| `idAndBlockHashes_all` | Block hash per group id (from indexer) |
| `txHashSig` | Tx hash **before** Spark payload (version, type, transparent outputs set) |
| `additionalTxSize` | Extra bytes in tx not counted elsewhere |
| `version` | `SpendTransactionVersion::V1` or `V2` (see H2 doc) |
| `extensionCommitment` | 32-byte binding; zeros for plain spend; Spark Name uses `getSparkNameCommitment` |

**Outputs:**

| Parameter | Meaning |
|-----------|---------|
| `fee` | Computed fee |
| `serializedSpend` | Spark spend blob → tx extra payload |
| `outputScripts` | Additional output scripts (order preserved) |
| `spentCoinsOut` | Coins consumed |

Constants in `include/spark.h`: `TRANSACTION_SPARK` (9), `TRANSACTION_SPARK_V2` (11), `SPARK_TX_VERSION` (3), `SPARK_CHANGE_D` (default change diversifier).

### Spark Name

| Function | Role |
|----------|------|
| `getSparkNameCommitment(nameData)` | Extension commitment for the spend |
| `getSparkNameOwnershipMessage(digest, version)` | Ownership scalar for the proof |
| `GetSparkNameScript(nameData, ownershipDigest, version, ...)` | Final name script appended after spend data |
| `getSparkNameTxDataSize` | Size helper for fee / payload planning |

Flow: build a Spark spend tx, compute ownership digest from the incomplete tx hash, then append the Spark Name script to extra payload. See firo wallet `CreateSparkNameTransaction` for the full sequence.

**Indexer RPCs** (also on ElectrumX where supported):

- `getsparknames` — name, block height, P-address
- `getsparknamedata` — height + additional info for a name

---

## ElectrumX / indexer APIs

Mobile wallets typically sync via an ElectrumX-compatible server rather than a full node.

| Method | Arguments | Returns |
|--------|-----------|---------|
| `getsparkanonymityset` | `coinGroupId`, `startBlockHash` (empty = full set) | `blockHash`, `setHash`, `mints` (serialized coin + tx hash pairs) |
| `getsparkmintmetadata` | list of coin hashes | set id + block height per coin |
| `getusedcoinstags` | start index (0 = full set) | used linking tags after index |
| `getsparklatestcoinid` | — | latest coin group id |

Call `getsparkanonymityset` periodically to keep cover sets current. Pass `setHash` and `blockHash` into `createSparkSpendTransaction` keyed by `coin.groupId` / `nId`.

---

## Wallet creation

**First run** (wallet unlocked):

1. Derive 32-byte seed with BIP44 using `BIP44_SPARK_INDEX` (`0x6`) and `DEFAULT_SPARK_NCOUNT`.
2. `createSpendKey(SpendKeyData(seed, index))` → `createFullViewKey` → `createIncomingViewKey`.
3. **Persist** `FullViewKey` and `IncomingViewKey` (unencrypted is normal — needed for sync).
4. **Do not persist** `SpendKey`. Regenerate only when signing a spend.

**Later runs:** load view keys from DB into memory.

**Default P-address:** `getAddress(incomingViewKey, 1)`. Each diversifier maps to one P-address; track max diversifier or store address objects for an address book. Diversifiers can be recovered from synced coins.

---

## Wallet sync

### Mints

1. `getsparklatestcoinid` → upper bound for group ids.
2. For each group, `getsparkanonymityset` → store cover set + hashes.
3. For each new mint, `identifyCoin(coin, incomingViewKey)`:
   - Success → wallet coin; `getMetadata` → `CSparkMintMeta`.
   - `getsparkmintmetadata` for `nId`, height, etc.
4. Store **`lTagHash`** with each coin:

   ```cpp
   spark::IdentifiedCoinData id = identifyCoin(coin, incomingViewKey);
   spark::RecoveredCoinData recovered = coin.recover(fullViewKey, id);
   uint256 lTagHash = primitives::GetLTagHash(recovered.T);
   ```

   (`GetLTagHash` is in `src/primitives.h`.)

### Spends

1. `getusedcoinstags` (incremental from last index).
2. For each tag, hash and compare to stored `lTagHash` values.
3. On match → mark mint spent, update balance.

---

## Mobile integration stack

```
sparkmobile (C++)
    ↓ FFI
flutter_libsparkmobile
    ↓ Dart plugin
Stack Wallet (or other Flutter wallet)
```

1. Link **sparkmobile** and expose APIs through **flutter_libsparkmobile**.
2. Wallet handles: keys, sync, tx assembly, broadcast, UI.
3. For H2: follow [H2_INTEGRATION.md](H2_INTEGRATION.md) for `nType`, `spendVersion`, fee estimation, and Spark Name V2.

---

## Spats asset burn (future)

Spats introduces burn outputs (marker + value, no coin) for public asset supply tracking. Balance proof generation at “point 10” in the Spats spend section must include a **burn** term in both prover and verifier. Details depend on the Spats specification; not yet part of sparkmobile.

---

## Related docs

- [H2_INTEGRATION.md](H2_INTEGRATION.md) — Chaum V2, flutter_libsparkmobile, Stack Wallet
- [../include/spark.h](../include/spark.h) — public C++ API
- [../CLAUDE.md](../CLAUDE.md) — build and test
