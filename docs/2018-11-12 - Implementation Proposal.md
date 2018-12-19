# Updated Implementation Proposal


* Author: Martin Plattner (csat3127@student.uibk.ac.at)
* Status: Draft
* Created: 2018-12-07


## Table of Contents

<!-- MarkdownTOC autolink="true" autoanchor="true" -->

- [General](#general)
- [Relevant Numbers](#relevant-numbers)
- [Step 1: Add Basic Fork-Support](#step-1-add-basic-fork-support)
    - [\[TODO\] Chain and Fork Configuration](#todo-chain-and-fork-configuration)
        - [Root \(=Main/Parent\) Chain](#root-mainparent-chain)
        - [Forked Chain](#forked-chain)
    - [Parser Output Files](#parser-output-files)
- [Step 2: Adapt Parser to Support Forks](#step-2-adapt-parser-to-support-forks)
    - [chain/ Directory \(`ChainAccess`\)](#chain-directory-chainaccess)
        - [Issue: `Output.getSpendingTx()`](#issue-outputgetspendingtx)
    - [scripts/ Directory \(`ScriptAccess`\)](#scripts-directory-scriptaccess)
        - [Issue: txFirstSeen and txFirstSpent of `ScriptDataBase`](#issue-txfirstseen-and-txfirstspent-of-scriptdatabase)
    - [hashIndex/ Directory \(`HashIndex`\)](#hashindex-directory-hashindex)
    - [addressesDb/ Directory \(`AddressIndex`\)](#addressesdb-directory-addressindex)
    - [Parser Output Files](#parser-output-files-1)
- [Interface Changes](#interface-changes)
    - [Block](#block)
    - [Transaction](#transaction)
    - [Input](#input)
    - [Output](#output)
    - [Address](#address)
- [Testing](#testing)

<!-- /MarkdownTOC -->


<a id="general"></a>
## General
* This document outlines my planned approach, and is accompanied by a commit that contains my first code changes.
    - See https://github.com/martinplattnr/BlockSci/commit/1d5d868f337d9d5942d43b76d56d8278a253eadd
* To break down things, I want to integrate fork-support in two steps:
    1. **Enable loading of two separately parsed chains with limited features.** (Without parser modification.)
    2. **Adapt the parser to parse forks (on top) of parent chains.** (script/, addressDb/, hashIndex/ files)
* I try to optimize memory usage, not disk usage.
* I marked some parts as [TODO] when I have no solution for them yet. Inputs are welcome.

<a id="relevant-numbers"></a>
## Relevant Numbers

Some numbers of BTC that are relevant to estimate performance and storage/memory usage of the proposed changes:

**At the block height of the BCH Fork (478,558):**

* **Transactions:** 243,276,437 × 16 byte = 3.9GB (`RawTransaction` data)
* **Inputs:** 609,169,714 × 16 byte = 9,8GB (`Inout` data)
* **Outputs:** 662,670,387 × 16 byte = 10.6GB (`Inout` data)
* Total: 24,3 GB (= chain/tx_data.dat file)

**At the current block height (551,912):**

* **Transactions:** 360,633,065 × 16 byte = 5.8GB (`RawTransaction` data)
* **Inputs:** 908,752,265 × 16 byte = 14.5GB (`Inout` data)
* **Outputs:** 980,410,005 × 16 byte = 15.7GB (`Inout` data)
* Total: 36GB (= chain/tx_data.dat file)

<a id="step-1-add-basic-fork-support"></a>
## Step 1: Add Basic Fork-Support

* This step involves adding the foundation to support forks: Configuration (`DataAccess`, `ChainConfiguration` etc.), loading of chains, traversing blocks and transactions for both the main chain and forked chains.
* The parser is not modified in this step such that I can get familiar with the code without straight touching the (more complex) parser code.
* Enabling fork-support that allows iterating blocks and transactions should still be possible.
* The intended procedure is as follows:
    1. BTC and BCH are parsed individually to separate parser output directories.
    2. The config file of the main chain (BTC) points to the data directory of the forked chain (BCH), and vice versa.
    3. BlockSci is modified to load both chains and support traversing of blocks and transactions. The forked chain re-uses the block/tx-data from the parent chain.
    4. This should work without changing the parser. Thus, some hash/script/address-related code won't work correctly yet, also following the transaction graph (getSpendingTx) won't work, as that all needs parser modification.
    5. In pseudo code, what should work is the following:
    ```python
    # internally load and use the pre-fork data of BTC 
    blockchain = blocksci.BlockChain("/path/to/bch")
    for block in blocks:
        for tx in block:
            # do something with the transaction
            # data from >= fork block height is loaded from the /path/to/bch dir
            # data from < fork block height is loaded from the /path/to/btc dir
    ```


<a id="todo-chain-and-fork-configuration"></a>
### [TODO] Chain and Fork Configuration

* Every chain (whether it is a fork or a root/parent chain) has its own parser output directory and its own config file.
    - Even though not all current directories (script/ hashIndex/) will be used for all chains, see Step 2
    - When we phoned I had the impression that you prefer the directories being separated. Let me know if your opinion has changed.
* Most relevant BlockSci classes have access to its chain's `DataAccess` object. Thus, I'd make sense to add the fork configuration in there.
    - Regarding the JSON config file, data can be added to chainConfig and its corresponding class `ChainConfiguration`, also accessible via `DataAccess`.
* However, I could not yet come up with a clean solution to storing (how and where) the nested chain configs.
    - Assume a user has parsed BTC -> BCH -> BSV but just wants to load up BCH using `new Blockchain("path/to/bch")`
    - The intended result is a single Blockchain object, but the config of (at least) all parent chain(s) upto and including the root chain have to be loaded as well, in order to load the required parts of parent chain data. Doing this results in a tree of chains (or chain configs).
    - I see various options, but have yet to figure out what is most appropriate. Any ideas?
        + BCH's `DataConfiguration` object held by the `DataAccess` object could contain a `DataConfiguration.children` as well as `DataConfiguration.parent` to reflect the a tree of objects.
* Information that is required to load/parse a fork:
    1. Block height at which the fork occured
    2. Other needed information that can be derived from i. and existing blockchain data:
        - Tx index of the first "new" (= not shared) transaction
        - Input index of the first "new" transaction


<a id="root-mainparent-chain"></a>
#### Root (=Main/Parent) Chain

* Added keys:
    - `chainConfig.forkDataDirectories`: Set the parser output directories that contain data of direct forks.

**/storage/blocksci/bitcoin/config.json:**
```json
{
    "chainConfig": {
        "coinName": "bitcoin",
        "dataDirectory": "/storage/blocksci/bitcoin",
        "pubkeyPrefix": [0],
        "scriptPrefix": [5],
        "segwitActivationHeight": 481824,
        "segwitPrefix": "bc",

        "### NEW ENTRY ###",
        "forkDataDirectories": [
            "/storage/blocksci/bitcoin-cash",
            "/storage/blocksci/bitcoin-gold",
            "..."
        ],
    },
    "parser": {
        "disk": {
            "blockMagic": 3652501241,
            "coinDirectory": "/storage/nodes/bitcoin",
            "hashFuncName": "doubleSha256"
        },
        "maxBlockNum": -6
    },
    "version": 6
}
```


<a id="forked-chain"></a>
#### Forked Chain

* Added keys:
    - `chainConfig.parentChainDirectory`: Set the parser output directory of the parent chain
    - `chainConfig.forkBlockHeight`: Set the block height at which the fork occured
    - `chainConfig.forkDataDirectories`: (Optionally) set the parser output directories of additional forks

**/storage/blocksci/bitcoin-cash/config.json:**
```json
{
    "chainConfig": {
        "coinName": "bitcoin_cash",
        "dataDirectory": "/storage/blocksci/bitcoin-cash",
        "pubkeyPrefix": [0],
        "scriptPrefix": [5],
        "segwitActivationHeight": 2147483647,
        "segwitPrefix": "NONE",

        "### NEW ENTRIES ###",
        "parentChainDirectory": "/storage/blocksci/bitcoin",
        "forkBlockHeight": 478558,
        "forkDataDirectories (optional)": [
            "/storage/blocksci/bitcoin-cash-sv"
        ],
    },
    "parser": {
        "disk": {
            "blockMagic": 3652501241,
            "coinDirectory": "/storage/nodes/bitcoin-cash",
            "hashFuncName": "doubleSha256"
        },
        "maxBlockNum": -6
    },
    "version": 6
}
```

<a id="parser-output-files"></a>
### Parser Output Files

Overview of the parser output files of step 1:

```
/storage/blocksci/bitcoin/
├── addressesDb              # unchanged, as not considered in step 1
├── chain                    # chain data of BTC, in the current format of BlockSci
│   ├── block.dat
│   └── ...
├── config.json              # contains "links" to the directories of all forks (forkDataDirectories)
│                            #   here: link to /storage/blocksci/bitcoin-cash, as shown below
├── hashIndex                # unchanged, as not considered in step 1
├── mempool                  # unchanged, as not considered in step 1
├── parser                   # unchanged, as not considered in step 1
└── scripts                  # unchanged, as not considered in step 1


/storage/blocksci/bitcoin-cash/
├── addressesDb              # unchanged, as not considered in step 1
├── chain                    # chain data of BTC, in the current format of BlockSci
│   └── ...
├── config.json              # contains a "link" to the parent chain directory (parentChainDirectory)
│                            #   here: /storage/blocksci/bitcoin/
└── ...
```



<a id="step-2-adapt-parser-to-support-forks"></a>
## Step 2: Adapt Parser to Support Forks

Step 2 involves the adaption of the parser to parse forks correctly.

The proposed process starts by parsing a user-specified root chain, eg. BTC. After that, forks of BTC can be added to the configuration, either manually or via a command-line option. Parser `update` calls should then parse all configured chains, including the root (BTC) chain. Added forks can be parsed "on top" of the existing data. Already existing data for other chains (eg. scripts) are reused, new data is added.

Forks have their own config file and parser output directory, but some of the parsed output data (eg. the `HashIndex`) is only stored in the directory of the root chain. The corresponding directories of the fork may remain empty.

<a id="chain-directory-chainaccess"></a>
### chain/ Directory (`ChainAccess`)

* All files can contain data for the entire individual chain
    - Fields like `Inout.toAddressNum` are adjusted to match the data of the fork
    - Optimization option: Only store new (= after fork) blocks/transactions etc. on disk. Not a priority.
* A fork uses the parent's files for data upto the fork block height, as already outlined in the first proposal
    - The idea is `if blockHgt < forkBlockHgt: parent.block(blockHgt) else: fork.block(blockHgt)`
        + The approach is similar for most `ChainAccess` methods
* Open question: Is it better to change the current `ChainAccess` itself, or create a derived class like `ChainAccessFork`? I guess the decision will (also) depend a lot on performance, as having a `ChainAccess*` pointer in `DataAccess` that can point to both `ChainAccess` and `ChainAccessFork` may introduce expensive indirection. For now, In the commited code I edited `ChainAccess` itself.


<a id="issue-outputgetspendingtx"></a>
#### Issue: `Output.getSpendingTx()`

**Problem:**
Given that BTC's transactions and its in-/outputs are stored in tx_data.dat, assume that the BCH instance should re-use this data (upto the fork block height).
Regarding `Output.getSpendingTx()`, re-using data from the parent chain works correctly for all outputs that are spent _before_ the fork. However, for outputs that are _unspent (UTXO) at the time of the fork_, separate storage of the `Inout.linkedTxNum` for the forked chain is needed, as the output can be spent in both chains individually.


**Proposed Solutions**

The order represents my preference in descending order. The additional storage requirements are calculated for a fork of BTC at the block height of the BCH fork.

1. Duplicate just the `uint32_t Inout.linkedTxNum` field for all outputs of the parent chain upto the fork block height
    - Add a new memory-mapped file that contains the `Inout.linkedTxNum` fields of _all_ pre-fork transactions, eg. chain/inoutLinkedTxNum.dat
    - The root chain can use the original `Inout` data in /rootchain/chain/tx_data.dat, and every fork has a separate inoutLinkedTxNum.dat file for each of its parent chains.
    - Pros: less storage overhead than 3., namely 662,670,387 pre-fork outputs × 4 bytes = 2.7GB per chain *
    - Cons: more complex to implement than 3. (and 2.), as the Inout data is "split", similar as transaction data is stored in various locations and put together in `TxData`.
2. Duplicate only affected (=_unspent-at-fork-height_ outputs) `Inout` objects (or just the `Inout.linkedTxNum`) for the fork upto the fork block height. This can't be done with a memory mapped file, as the data is scattered and can't be indexed by offset.
    1. An option would be to serialize the entire C++ object that holds this data - similar to the `ChainIndex` class, with its `std::unordered_map<blocksci::uint256, BlockType>`.)
    2. Another option is to save the data into RocksDB, but I guess that hurts performance (too much)?
3. Duplicate _all_ Inouts for the fork upto the fork block height
    - Eg. a separate chain/inout.dat file, but without RawTransaction data.
    - Pros: easy to implement, faster than 3.
    - Cons: large storage and memory overhead and thus, most likely not a good choice  
      (662,670,387 × 16 byte = 10.6GB *)

Do you have any ad-hoc thoughts or ideas on that issue and the proposed options?

\* Calculated for the BCH fork, see [Relevant Numbers](#relevant-numbers) Section


<a id="scripts-directory-scriptaccess"></a>
### scripts/ Directory (`ScriptAccess`)

* The scripts in scripts/ are already de-duplicated in the current version of BlockSci.
* _One single_ scripts/ directory could be used for all related chains, stored in the directory of the root (top-most) chain, eg. /storage/bitcoin/scripts
* Script data can be reused for all related chains
    - This is done by setting the `Inout.toAddressNum` to the correct scriptNum for all related chains during parsing
    - If a new script in any of the related chains comes along, a new ID is assigned, otherwise data is reused

<a id="issue-txfirstseen-and-txfirstspent-of-scriptdatabase"></a>
#### Issue: txFirstSeen and txFirstSpent of `ScriptDataBase`

* The `uint32_t ScriptDataBase.txFirstSeen` and `uint32_t ScriptDataBase.txFirstSpent` fields may be different for every fork.
* I think this applies to `uint32_t ScriptDataBase.typesSeen` as well, right? I'm not sure what typesSeen is for. Probably which address types have been seen for the given public key?
* **Solution:** Create separate file(s) for txFirstSeen and txFirstSpent for every chain and address type.
    - scripts/pubkey_script_firstSeenSpent.dat; or separate files for firstSeen and firstSpent
    - scripts/scripthash_script_firstSeenSpent.dat; or separate files for firstSeen and firstSpent
    - etc.
    - Do you prefer separate files for both fields, or combine them into one file per type?
    - Required additional storage: approx. +5GB needed for every assumed BTC fork at the current height
        + 41,636,537,440 bytes (current pubkey_script.dat size) / 80 (size(`PubkeyData`)) × 4 bytes × 2 variables
        + Storage requirement for other script types is minor


<a id="hashindex-directory-hashindex"></a>
### hashIndex/ Directory (`HashIndex`)

* All `HashIndex` data can be stored in the root (top-most) chain data directory. Thus, there is just one `HashIndex` for all related chains. The forked chains access the `HashIndex` of the root chain or have their own objects to the same index.

The `HashIndex` currently contains:

1. **txHash -> txNum**
    - Required changes: Add a `chainId` byte to the key as a suffix, such that the database becomes (txHash, chainId) -> txNum. This is needed to retrieve duplicate (replayed) transactions for all related chains. (`Transaction.getDuplicates()` and `Transaction.hasDuplicates()`)
        + The chain id is fixed for every chain, see include/blocksci/core/chain_ids.hpp. Eg. BTC=1, BTC_TEST=2, ... Is a 1-byte chain ID enough or should I go with 2 bytes for future compatibility?
2. **address identifier (= pubkey, pubkey hash, or script hash etc.) -> scriptNum**
    - Required changes: I think none, as address identifiers are equal accross related chains and are represented as the same script and thus, have the same scriptNum. Eg. the same public key can be used accross all chains, stored as one scriptNum that is used for all related chains.


<a id="addressesdb-directory-addressindex"></a>
### addressesDb/ Directory (`AddressIndex`)

* As for hashIndex/ and Script/ I think _one_ addressesDb/ directory can be used for all related chains.

The `AddressIndex` currently contains:

1. **Address -> \[Outputs sent to this address\]** 
    - Internally just the key (scriptNum, txNum, outputNumInTx), values are empty
    - Required changes: This seems tricky. The scriptNums are reused accross all related chains, so for every scriptNum we need to distinguish for which chain we are adding an output. The chainId field could be added after scriptNum, resulting in the key (scriptNum, chainid, txNum, outputNumInTx). However, that would essentially result in a lot of redundant data. When a new fork is added, the exsting entries of the parent chain have to be duplicated with the chainId of the fork.
2. **[TODO] Address ---(is nested in)---> Address (as DedupAddress):**
    - Internally (scriptNum) -> (parent scriptNum, parent scriptType)
    - Required changes: No ideas yet.




<a id="parser-output-files-1"></a>
### Parser Output Files

How the parser output files are used, changed, or added in step 2. New files are marked as `*NEW`.

```
btc_parser_output/
├── addressesDb              # 
├── [ 59G]  chain
│   ├── [ 46M]  block.dat
│   ├── [ 26M]  coinbases.dat
│   ├── [2.7G]  firstInput.dat
│   ├── [2.7G]  firstOutput.dat
│   ├── [1.7G]  input_out_num.dat
│   ├── [3.4G]  sequence.dat
│   ├── [ 33G]  tx_data.dat
│   ├── [*NEW]  inoutLinkedTxNum.dat
│   ├── [ 11G]  tx_hashes.dat
│   ├── [2.7G]  tx_index.dat
│   └── [1.3G]  tx_version.dat
│
├── config.json              # contain "links" to the directories of all forks, here just bch_parser_output/
├── hashIndex                # 
├── mempool                  # 
├── parser                   # 
└── [ 45G]  scripts          # files contain the scripts of _all_ related chains, not just of BTC
    ├── [1.5G]  multisig_script_data.dat
    ├── [435M]  multisig_script_index.dat
    ├── [ 52M]  nonstandard_script_data.dat
    ├── [8.6M]  nonstandard_script_index.dat
    ├── [385M]  null_data_script_data.dat
    ├── [ 65M]  null_data_script_index.dat
    ├── [ 39G]  pubkey_script.dat
    ├── [*NEW]  pubkey_firstSeenSpent.dat
    ├── [3.9G]  scripthash_script.dat
    └── [*NEW]  scripthash_firstSeenSpent.dat

bch_parser_output/
├── addressesDb                       # 
├── chain                             # chain data of BCH, all data remains as it currently is
│   ├── ...                           # all existing chain/ files, see above
│   └── [*NEW]  inoutLinkedTxNum.dat  # the Inout.linkedTxNum field for pre-fork in- and ouputs
│
├── config.json                       # contains a link to the parent chain directory, here just btc_parser_output/
│
├── hashIndex                         # only the hashIndex of the root chain is used, so this directory is empty
└── scripts                           # only scripts/ of the root chain is used, so this directory is empty
```

An option to consider would be to also maintain separate hashIndex and addressesDb for every fork - as if the fork has been parsed individually without including related chains. That would allow loading _only_ the forked chain without having the performance penalty of the fork extension.

<a id="interface-changes"></a>
## Interface Changes

* The same (and more) proposed interface changes can be found on GitHub.
    - See https://github.com/martinplattnr/BlockSci/commit/1d5d868f337d9d5942d43b76d56d8278a253eadd
* This part still needs work and is not final yet. Feedback is welcome.
* Some methods offer variants with a `uint8_t chainId` and `std::vector<uint8_t chainIds` parameter to limit the method to one or more related chains
* Some methods return `std::vector<std::pair<uint8_t, [SomeClass]>>`, where uint8_t represents the chain id
    - I'm not sure whether this is a good idea, an alternative would be to return `std::vector<SomeClass>` without the grouping by chain id. My current proposal is not yet 100% consistent with regards to this issue.


<a id="block"></a>
### Block

* `uint8_t getChainId() const`: 1-byte ID for every chain, eg. BTC=1, BTC_TEST=2, LITECOIN=5, ...
    - This method is added to all of the following classes and returns the chain id of the given object
    - This ID (byte) is used in the data structures, eg. in the hashIndex as a suffix to the tx hash
    - At the moment the IDs are stored in include/blocksci/core/chain_ids.hpp, I'm not sure if the current static-consts-inside-a-class approach is a good choice, but it's trivial to change it.


<a id="transaction"></a>
### Transaction

* `std::vector<Transaction> getDuplicates() const`  get transactions with the same tx hash on another chain (forks without replay protection). can be implemented as a range query on HashIndex.
* `Transaction getDuplicates(uint8_t chainId) const`
* `Transaction getDuplicates(std::vector<uint8_t> chainIds) const`
* `bool hasDuplicates() const`: check whether other transactions with the same tx hash exist. can be implemented as a range query on HashIndex as well
* `uint8_t getChainId() const`


<a id="input"></a>
### Input

* `Transaction getSpentTx(uint8_t chainId) const`
* `std::vector<std::pair<uint8_t, Transaction>> getSpentTxes()`
* `std::vector<std::pair<uint8_t, Transaction>> getSpentTxes(uint8_t chainId) const`
* `std::vector<std::pair<uint8_t, Transaction>> getSpentTxes(std::vector<uint8_t> chainIds) const`
* `OutputPointer getSpentOutputPointers() const`
* `OutputPointer getSpentOutputPointers(uint8_t chainId) const`
* `OutputPointer getSpentOutputPointers(std::vector<uint8_t> chainIds) const`
* `std::vector<std::pair<uint8_t, Transaction>> getSpentTxes() const`
* `std::vector<std::pair<uint8_t, Transaction>> getSpentTxes(std::vector<uint8_t> chainIds) const`
* `uint8_t getChainId() const`

<a id="output"></a>
### Output

* `std::vector<uint32_t> getSpendingTxIndexes() const`
* `std::vector<std::pair<uint8_t, Transaction>> getSpendingTxes() const`
* `std::vector<std::pair<uint8_t, Input>> getSpendingInputs() const`
* `std::vector<std::pair<uint8_t, InputPointer>> getSpendingInputPointers() const`
* `uint8_t getChainId() const`

<a id="address"></a>
### Address

* `std::vector<std::pair<uint8_t, ranges::any_view<Output>>> getOutputs(std::vector<uint8_t> chainIds) const`
* `std::vector<Output> getOutputs(uint8_t chainId) const`
* `std::vector<std::pair<uint8_t, ranges::any_view<Input>>> getInputs(std::vector<uint8_t> chainIds) const`
* `std::vector<Input> getInputs(uint8_t chainId) const`
* `std::vector<std::pair<uint8_t, int64_t>> calculateBalances(BlockHeight height) const`: get balances for all chains by chainId


<a id="testing"></a>
## Testing

I wondered what's the best way to test my extension (once I'm there). I think working with the original chain is cumbersome on a development machine (16GB RAM), as BCH only forks at block height 478,558 and thus, a lot of data has to be loaded/parsed after every code modification.

* Do you have any input on that, what data do you use for testing and development? Local? On a server?
* Maybe I can use BTC and BCH testnets, if the fork is reflected there nicely, I'm not sure.
* Malte's new testchain-generator also looks interesting. Maybe I can extend it and use it to generate 2 chains for testing purposes, what do you think?


