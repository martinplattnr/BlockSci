//
//  chain_access.hpp
//  blocksci
//
//  Created by Harry Kalodner on 7/26/17.
//
//

#ifndef chain_access_hpp
#define chain_access_hpp

#include "file_mapper.hpp"
#include "exception.hpp"

#include <blocksci/core/bitcoin_uint256.hpp>
#include <blocksci/core/core_fwd.hpp>
#include <blocksci/core/raw_block.hpp>
#include <blocksci/core/raw_transaction.hpp>
#include <blocksci/core/typedefs.hpp>
#include <blocksci/core/transaction_data.hpp>
#include <blocksci/chain/output_pointer.hpp>
#include "data_configuration.hpp"
#include "chain_access.hpp"

#include <wjfilesystem/path.h>

#include <algorithm>
#include <iostream>
#include <memory>

namespace blocksci {

    /** Provides data access for blocks, transactions, inputs, and outputs.
     *
     * The files here represent the core data about blocks and transactions.
     * Data is stored in a hybrid column and row based structure.
     * The core tx_data.dat and tx_index.dat are the core data files containing most information that is
     * used about transactions, inputs, and outputs. The other files in this folder are column oriented storing
     * less frequently accessed values.
     *
     * Directory: chain/
     */
    class ChainAccess {
        /** Stores the serialized RawBlock data in the same order they occur in the blockchain, indexed by block height.
         *
         * File: chain/block.dat
         * Raw data format: [<RawBlock>, <RawBlock>, ...]
         */
        FixedSizeFileMapper<RawBlock> blockFile;

        /** Stores the coinbase data for every block, indexed by block height.
         *
         * Accessed by file offset that is stored for every block in Rawblock.coinbaseOffset
         *
         * File: chain/coinbases.dat
         * Raw data format: [uint32_t coinbaseLength, <coinbaseBytes>, uint32_t coinbaseLength, <coinbaseBytes>, ...]
         * Example: Genesis block of BTC contains "[...] The Times 03/Jan/2009 Chancellor on brink of second bailout for banks"
         */
        SimpleFileMapper<> blockCoinbaseFile;

        /** Stores the serialized RawTransaction data in the same order they occur in the blockchain, indexed by tx number.
         * Implemented as IndexedFileMapper as RawTransaction objects have variable length due to variable no. of inputs/outputs.
         *
         * The scripts and addresses are not stored in this file.
         * Instead, a scriptNum is stored that references to a scripts file, @link blocksci::ScriptAccess.
         *
         * Files: - chain/tx_data.dat: actual data
         *        - chain/tx_index.dat: index file that stores the offset for every transaction
         * Raw data format: [<RawTransaction>, <Inout list>, <RawTransaction>, <Inout list>, ...]
         */
        IndexedFileMapper<mio::access_mode::read, RawTransaction> txFile;

        /** Stores the version number that is stored in the first 4 bytes of every transaction on the blockchain, indexed by tx number.
         *
         * File: chain/tx_version.dat
         * Raw data format: [<int32_t versionNumberOfTx0>, <int32_t versionNumberOfTx1>, ...]
         */
        FixedSizeFileMapper<int32_t> txVersionFile;

        /** Stores the blockchain-wide number of the first input for every transaction, indexed by tx number.
         *
         * File(s): - chain/firstInput.dat
         * Raw data format: [<uint64_t firstInputNumberOfTx0>, <uint64_t firstInputNumberOfTx1>, ...]
         */
        FixedSizeFileMapper<uint64_t> txFirstInputFile;

        /** Stores the blockchain-wide number of the first output for every transaction, indexed by tx number.
         *
         * File: chain/firstOutput.dat
         * Raw data format: [<uint64_t firstOutputNumberOfTx0>, <uint64_t firstOutputNumberOfTx1>, ...]
         */
        FixedSizeFileMapper<uint64_t> txFirstOutputFile;

        /** Stores the tx-internal output number of the spent output for every input, indexed by blockchain-wide input number.
         *
         * File: chain/input_out_num.dat
         * Raw data format: [<uint16_t>, <uint16_t>, ...]
         */
        FixedSizeFileMapper<uint16_t> inputSpentOutputFile;

        /** Stores the blockchain field sequence number for every input, indexed by blockchain-wide input number.
         *
         * File: chain/sequence.dat
         * Raw data format: [<uint32_t sequenceFieldOfInput0>, <uint32_t sequenceFieldOfInput1>, ...]
         */
        FixedSizeFileMapper<uint32_t> sequenceFile;

        /** Stores a mapping of (tx number) to (tx hash), thus indexed by tx number.
         *
         * File: chain/tx_hashes.dat
         * Raw data format: [<uint64_t firstOutputNumberOfTx0>, <uint64_t firstOutputNumberOfTx1>, ...]
         */
        FixedSizeFileMapper<uint256> txHashesFile;

        /** Stores a mapping of (output number) to (spending tx number) for forks before the fork */
        FixedSizeFileMapper<uint32_t> preForkLinkedTxNumFile;

        /** Hash of the last loaded block (copy of the hash) */
        uint256 lastBlockHash;

        /** Hash of the last loaded block (pointer; is used to compare *pointer to lastBlockHash to detect block reorgs) */
        const uint256 *lastBlockHashDisk = nullptr;

        /** Number of the highest loaded block, including genesis block, eg. is 2 if 1 block is mined on the genesis block */
        BlockHeight maxHeight = 0;

        /** Number of the highest loaded transaction */
        uint32_t _maxLoadedTx = 0;

        /** Block height that BlockSci should load up to as a 1-indexed number
         * Eg. 10 loads blocks [0, 9], and -6 loads all but the last 6 blocks */
        BlockHeight blocksIgnored = 0;

        bool errorOnReorg = false;

        std::unique_ptr<ChainAccess> parentChain;
        // std::vector<std::unique_ptr<ChainAccess>> forkedChains;

        

        void reorgCheck() const {
            if (errorOnReorg && lastBlockHash != *lastBlockHashDisk) {
                throw ReorgException();
            }
        }

        void setup() {
            if (blocksIgnored <= 0) {
                // todo: add -1 to have maxHeight=0 if only the genesis block is loaded
                maxHeight = static_cast<BlockHeight>(blockFile.size()) + blocksIgnored;
            } else {
                maxHeight = blocksIgnored;
            }

            // set firstForkedTxIndex from firstForkedBlockHeight
            // todo-fork: check for off-by-1 error
            if (maxHeight >= firstForkedBlockHeight && firstForkedBlockHeight > BlockHeight(0)) {
                auto firstForkedBlock = blockFile[firstForkedBlockHeight];
                firstForkedTxIndex = firstForkedBlock->firstTxIndex;
                //std::cout << "firstForkedTxIndex = " << firstForkedTxIndex << std::endl;
                forkInputIndex = 0; // todo-fork
            }

            if (maxHeight > BlockHeight(0)) { // todo: change to >=
                //std::cout << "first blockFile access from " << getChainType() << " for height " << static_cast<OffsetType>(maxHeight) - 1 << std::endl;
                auto maxLoadedBlock = blockFile[static_cast<OffsetType>(maxHeight) - 1]; // todo: remove - 1
                // auto maxLoadedBlock = getBlock(maxHeight - 1);
                lastBlockHash = maxLoadedBlock->hash;
                _maxLoadedTx = maxLoadedBlock->firstTxIndex + maxLoadedBlock->txCount;
                lastBlockHashDisk = &maxLoadedBlock->hash;
            } else {
                lastBlockHash.SetNull();
                _maxLoadedTx = 0;
                lastBlockHashDisk = nullptr;
            }

            if (_maxLoadedTx > txFile.size()) {
                std::stringstream ss;
                ss << "Block data corrupted. Tx file has " << txFile.size() << " transaction, but max tx to load is tx " << _maxLoadedTx;
                throw std::runtime_error(ss.str());
            }

            /*
            std::cout << "setup(): " << std::endl;
            std::cout << "  - type = " << getChainType() << std::endl;
            std::cout << "  - firstForkedBlockHeight = " << firstForkedBlockHeight << std::endl;
            std::cout << "  - firstForkedTxIndex = " << firstForkedTxIndex << std::endl << std::flush;
            std::cout << std::endl << std::endl;
            */
        }

    public:
        BlockHeight firstForkedBlockHeight = 0;
        uint32_t firstForkedTxIndex = 0;
        uint64_t forkInputIndex = 0;

        explicit ChainAccess(DataConfiguration& config_) :
        blockFile(blockFilePath(config_.chainDirectory())),
        blockCoinbaseFile(blockCoinbaseFilePath(config_.chainDirectory())),
        txFile(txFilePath(config_.chainDirectory())),
        txVersionFile(txVersionFilePath(config_.chainDirectory())),
        txFirstInputFile(firstInputFilePath(config_.chainDirectory())),
        txFirstOutputFile(firstOutputFilePath(config_.chainDirectory())),
        inputSpentOutputFile(inputSpentOutNumFilePath(config_.chainDirectory())),
        sequenceFile(sequenceFilePath(config_.chainDirectory())),
        txHashesFile(txHashesFilePath(config_.chainDirectory())),
        preForkLinkedTxNumFile(preForkLinkedTxNumFilePath(config_.chainDirectory())),
        blocksIgnored(config_.blocksIgnored),
        errorOnReorg(config_.errorOnReorg),
        firstForkedBlockHeight(config_.chainConfig.firstForkedBlockHeight) {
            //std::cout << "Initializing ChainAccess only with DataConfiguration." << std::endl;
            //std::cout << "config_.chainDirectory(): " << config_.chainDirectory() << std::endl;

            if (config_.parentDataConfiguration) {
                //std::cout << "  parent chain config is available" << std::endl;
                this->parentChain = std::make_unique<ChainAccess>(*config_.parentDataConfiguration);
            }

            setup();
        }

        explicit ChainAccess(const filesystem::path &baseDirectory, BlockHeight blocksIgnored, bool errorOnReorg) :
        blockFile(blockFilePath(baseDirectory)),
        blockCoinbaseFile(blockCoinbaseFilePath(baseDirectory)),
        txFile(txFilePath(baseDirectory)),
        txVersionFile(txVersionFilePath(baseDirectory)),
        txFirstInputFile(firstInputFilePath(baseDirectory)),
        txFirstOutputFile(firstOutputFilePath(baseDirectory)),
        inputSpentOutputFile(inputSpentOutNumFilePath(baseDirectory)),
        sequenceFile(sequenceFilePath(baseDirectory)),
        txHashesFile(txHashesFilePath(baseDirectory)),
        preForkLinkedTxNumFile(preForkLinkedTxNumFilePath(baseDirectory)),
        blocksIgnored(blocksIgnored),
        errorOnReorg(errorOnReorg) {
            setup();
        }
        
        static filesystem::path txFilePath(const filesystem::path &baseDirectory) {
            return baseDirectory/"tx";
        }

        static filesystem::path preForkLinkedTxNumFilePath(const filesystem::path &baseDirectory) {
            return baseDirectory/"pre_fork_linked_tx_num";
        }
        
        static filesystem::path txHashesFilePath(const filesystem::path &baseDirectory) {
            return baseDirectory/"tx_hashes";
        }

        static filesystem::path blockFilePath(const filesystem::path &baseDirectory) {
            return baseDirectory/"block";
        }

        static filesystem::path blockCoinbaseFilePath(const filesystem::path &baseDirectory) {
            return baseDirectory/"coinbases";
        }

        static filesystem::path txVersionFilePath(const filesystem::path &baseDirectory) {
            return baseDirectory/"tx_version";
        }

        static filesystem::path firstInputFilePath(const filesystem::path &baseDirectory) {
            return baseDirectory/"firstInput";
        }

        static filesystem::path firstOutputFilePath(const filesystem::path &baseDirectory) {
            return baseDirectory/"firstOutput";
        }

        static filesystem::path sequenceFilePath(const filesystem::path &baseDirectory) {
            return baseDirectory/"sequence";
        }

        static filesystem::path inputSpentOutNumFilePath(const filesystem::path &baseDirectory) {
            return baseDirectory/"input_out_num";
        }
        bool hasParentChain() const {
            return this->parentChain ? true : false;
        }
        
        BlockHeight getBlockHeight(uint32_t txIndex) const {
            reorgCheck();
            if (errorOnReorg && txIndex >= _maxLoadedTx) {
                throw std::out_of_range("Transaction index out of range");
            }

            auto currentBlock = getBlock(0);
            while (txIndex >= currentBlock->firstTxIndex + currentBlock->txCount) {
                currentBlock++;

                // if first forked block is reached, get new block pointer that points to the forked chain
                if (static_cast<BlockHeight>(currentBlock->height) == firstForkedBlockHeight + 1) {
                    currentBlock = getBlock(firstForkedBlockHeight);
                }
            }

            // -1 to adjust for RawBlock.height, which is 1-indexed, but this method has always returned 0-indexed block heights
            return static_cast<BlockHeight>(currentBlock->height - 1);
        }

        const RawBlock *getBlock(BlockHeight blockHeight) const {
            reorgCheck();

            if (blockHeight >= firstForkedBlockHeight) {
                //std::cout << getChainType() << ".getBlock(" << blockHeight << ")" << std::endl;
                return blockFile[static_cast<OffsetType>(blockHeight)];
            }
            else if (parentChain) {
                return parentChain->getBlock(blockHeight);
            }
            throw std::runtime_error("Whoopsie. Something went terribly wrong");
        }

        const uint256 *getTxHash(uint32_t index) const {
            reorgCheck();

            if (index >= firstForkedTxIndex) {
                //std::cout << getChainType() << ".getTxHash(" << index << ")" << std::endl;
                return txHashesFile[index];
            }
            else if (parentChain) {
                return parentChain->getTxHash(index);
            }
            throw std::runtime_error("Whoopsie. Something went terribly wrong");
        }

        const RawTransaction *getTx(uint32_t index) const {
            reorgCheck();

            if (index >= firstForkedTxIndex) {
                //std::cout << getChainType() << ".getTx(" << index << ")" << std::endl;
                return txFile.getData(index);
            }
            else {
                return parentChain->getTx(index);
            }
            throw std::runtime_error("Whoopsie. Something went terribly wrong");
        }

        const int32_t *getTxVersion(uint32_t index) const {
            reorgCheck();

            if (index >= firstForkedTxIndex) {
                return txVersionFile[index];
            }
            else if (parentChain) {
                return parentChain->getTxVersion(index);
            }
            throw std::runtime_error("Whoopsie. Something went terribly wrong");
        }

        const uint32_t *getSequenceNumbers(uint32_t index) const {
            reorgCheck();

            if (index >= firstForkedTxIndex) {
                //std::cout << getChainType() << ".getSequenceNumbers(" << index << ")" << std::endl;
                return sequenceFile[static_cast<OffsetType>(getFirstInputNumber(index))];
            }
            else if (parentChain) {
                return parentChain->getSequenceNumbers(index);
            }
            throw std::runtime_error("Whoopsie. Something went terribly wrong");
        }

        const uint16_t *getSpentOutputNumbers(uint32_t index) const {
            reorgCheck();

            if (index >= firstForkedTxIndex) {
                //std::cout << getChainType() << ".getSpentOutputNumbers(" << index << ")" << std::endl;
                return inputSpentOutputFile[static_cast<OffsetType>(getFirstInputNumber(index))];
            }
            else if (parentChain) {
                return parentChain->getSpentOutputNumbers(index);
            }
            throw std::runtime_error("Whoopsie. Something went terribly wrong");
        }

        /** Get TxData object for given tx number */
        TxData getTxData(uint32_t index) const {
            reorgCheck();
            // Blockchain-wide number of first input for the given tx
            //auto firstInputNum = static_cast<OffsetType>(*txFirstInputFile[index]);
            auto firstInputNum = static_cast<OffsetType>(getFirstInputNumber(index));
            const uint16_t *inputsSpent = nullptr;
            const uint32_t *sequenceNumbers = nullptr;
            // todo-fork: make fork-aware
            if (firstInputNum < inputSpentOutputFile.size()) {
                inputsSpent = inputSpentOutputFile[firstInputNum];
                sequenceNumbers = sequenceFile[firstInputNum];
            }
            return {                   // construct TxData object
                getTx(index),          // const RawTransaction *rawTx
                getTxVersion(index),    // const int32_t *version
                getTxHash(index),       // const uint256 *hash
                inputsSpent,           // const uint16_t *spentOutputNums
                sequenceNumbers        // const uint32_t *sequenceNumbers
            };
        }

        size_t txCount() const {
            return _maxLoadedTx;
        }

        uint64_t inputCount() const {
            if (_maxLoadedTx > 0) {
                auto lastTx = getTx(_maxLoadedTx - 1);
                //return *txFirstInputFile[_maxLoadedTx - 1] + lastTx->inputCount;
                return getFirstInputNumber(_maxLoadedTx - 1) + lastTx->inputCount;
            }

            return 0;
        }

        uint64_t outputCount() const {
            if (_maxLoadedTx > 0) {
                auto lastTx = getTx(_maxLoadedTx - 1);
                //return *txFirstOutputFile[_maxLoadedTx - 1] + lastTx->outputCount;
                return getFirstOutputNumber(_maxLoadedTx - 1) + lastTx->outputCount;
            }

            return 0;
        }

        BlockHeight blockCount() const {
            return maxHeight;
        }

        // coinbases file is very small (< 50MB), so data does not need to be shared between forks
        std::vector<unsigned char> getCoinbase(uint64_t offset) const {
            auto pos = blockCoinbaseFile.getDataAtOffset(static_cast<OffsetType>(offset));
            uint32_t coinbaseLength;
            std::memcpy(&coinbaseLength, pos, sizeof(coinbaseLength));
            uint64_t length = coinbaseLength;
            pos += sizeof(coinbaseLength);
            auto unsignedPos = reinterpret_cast<const unsigned char *>(pos);
            return std::vector<unsigned char>(unsignedPos, unsignedPos + length);
        }

        void reload() {
            blockFile.reload();
            blockCoinbaseFile.reload();
            txFile.reload();
            txFirstInputFile.reload();
            txFirstOutputFile.reload();
            txVersionFile.reload();
            inputSpentOutputFile.reload();
            txHashesFile.reload();
            sequenceFile.reload();

            /*
             * does currently not work, as parentChain is const
            if (parentChain) {
                parentChain->blockFile.reload();
                parentChain->blockCoinbaseFile.reload();
                parentChain->txFile.reload();
                parentChain->txHashesFile.reload();
                parentChain->sequenceFile.reload();
            }
            */
            setup();
        }

        uint64_t getFirstInputNumber(uint32_t index) const {
            if (index >= firstForkedTxIndex) {
                return *txFirstInputFile[index];
            }
            else if (parentChain) {
                return parentChain->getFirstInputNumber(index);
            }
            throw std::runtime_error("Whoopsie. Something went terribly wrong");
        }

        uint64_t getFirstOutputNumber(uint32_t index) const {
            if (index >= firstForkedTxIndex) {
                //std::cout << getChainType() << ".getFirstOutputNumber(" << index << ")" << std::endl;
                return *txFirstOutputFile[index];
            }
            else if (parentChain) {
                return parentChain->getFirstOutputNumber(index);
            }
            throw std::runtime_error("Whoopsie. Something went terribly wrong");
        }

        uint16_t getSpentOutputNumberByInputNumber(uint64_t index) const {
            if (index >= firstForkedTxIndex) {
                //std::cout << getChainType() << ".getSpentOutputNumberByInputNumber(" << index << ")" << std::endl;
                return *inputSpentOutputFile[index];
            }
            else if (parentChain) {
                return parentChain->getFirstOutputNumber(index);
            }
            throw std::runtime_error("Whoopsie. Something went terribly wrong");
        }

        const uint32_t *getPreForkLinkedTxNum(uint32_t txNum, uint16_t outputNum) const {
            uint64_t index = this->getFirstOutputNumber(txNum) + outputNum;
            return preForkLinkedTxNumFile[index];
        }

        // todo: not used, could be removed; if not, pointer.chainId is not considered (but should be?)
        uint32_t getLinkedTxNumFork(const OutputPointer &pointer, uint32_t &defaultValue) {
            if (pointer.txNum > firstForkedTxIndex) {
                return defaultValue;
            }
            ChainAccess* chainAccessTmp = this;
            while (chainAccessTmp->parentChain) {
                chainAccessTmp = chainAccessTmp->parentChain.get();
                if (pointer.txNum > chainAccessTmp->firstForkedTxIndex) {
                    uint64_t outputIndex = getFirstOutputNumber(pointer.txNum);
                    return *(chainAccessTmp->preForkLinkedTxNumFile[outputIndex + pointer.inoutNum]);
                }
            }

            return 0;
        }

        /*
        std::vector<std::unique_ptr<ChainAccess>> getRelatedChains() const {
            std::vector<std::unique_ptr<ChainAccess>> relatedChains = forkedChains;
            relatedChains.push_back(parentChain);
            return relatedChains;
        }
        */

        BlockHeight getFirstForkedBlockHeight() const {
            return firstForkedBlockHeight;
        }

        uint32_t getFirstForkedTxIndex() const {
            return firstForkedTxIndex;
        }

        const std::string getChainType() const {
            return parentChain ? "fork" : "root";
        }
    };
} // namespace blocksci

#endif /* chain_access_hpp */
