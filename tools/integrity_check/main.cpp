//
//  main.cpp
//
//  blocksci_check_integrity
//  Created by Malte Möser on 3/6/20.
//

#include <blocksci/chain/blockchain.hpp>
#include <blocksci/chain/block.hpp>
#include <blocksci/address.hpp>
#include <blocksci/script.hpp>

#include <internal/chain_access.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>
#include <internal/dedup_address_info.hpp>
#include <internal/hash_index.hpp>

#include <openssl/sha.h>

#include <clipp.h>

#include <iostream>

using namespace blocksci;

/**
 Compute a checksum over raw block data.
 */
uint256 compute_rawblock_hash(const ChainAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    for(int i = 0; i < access.blockCount(); i++) {
        const RawBlock *block = access.getBlock(i);
        SHA256_Update(&sha256, &block->baseSize, 4);
        SHA256_Update(&sha256, &block->bits, 4);
        SHA256_Update(&sha256, &block->coinbaseOffset, 8);
        SHA256_Update(&sha256, &block->firstTxIndex, 4);
        SHA256_Update(&sha256, &block->hash, 32);
        SHA256_Update(&sha256, &block->height, 4);
        SHA256_Update(&sha256, &block->inputCount, 4);
        SHA256_Update(&sha256, &block->nonce, 4);
        SHA256_Update(&sha256, &block->outputCount, 4);
        SHA256_Update(&sha256, &block->realSize, 4);
        SHA256_Update(&sha256, &block->timestamp, 4);
        SHA256_Update(&sha256, &block->txCount, 4);
        SHA256_Update(&sha256, &block->version, 4);
    }
    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
 Compute a checksum over block data.
 */
uint256 compute_block_hash(const Blockchain &chain) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    for (const auto& block : chain) {
        auto blockHash = block.getHash();
        SHA256_Update(&sha256, blockHash.begin(), blockHash.size());

        auto height = block.height();
        SHA256_Update(&sha256, &height, sizeof(height));

        auto version = block.version();
        SHA256_Update(&sha256, &version, sizeof(version));

        auto timestamp = block.timestamp();
        SHA256_Update(&sha256, &timestamp, sizeof(timestamp));

        auto bits = block.bits();
        SHA256_Update(&sha256, &bits, sizeof(bits));

        auto nonce = block.nonce();
        SHA256_Update(&sha256, &nonce, sizeof(nonce));

        auto baseSize = block.baseSize();
        SHA256_Update(&sha256, &baseSize, sizeof(baseSize));

        auto totalSize = block.totalSize();
        SHA256_Update(&sha256, &totalSize, sizeof(totalSize));

        auto virtualSize = block.virtualSize();
        SHA256_Update(&sha256, &virtualSize, sizeof(virtualSize));

        auto weight = block.weight();
        SHA256_Update(&sha256, &weight, sizeof(weight));

        auto sizeBytes = block.sizeBytes();
        SHA256_Update(&sha256, &sizeBytes, sizeof(sizeBytes));

        auto coinbase = block.getCoinbase();
        SHA256_Update(&sha256, coinbase.data(), coinbase.size());
    }
    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
 Compute a checksum over core raw transaction data.
 */
uint256 compute_txrawdata_hash(const ChainAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    for(size_t i = 0; i < access.txCount(); i++) {
        // no additional padding in RawTransaction, can simply hash struct
        const RawTransaction *tx = access.getTx(i);
        SHA256_Update(&sha256, tx, tx->serializedSize());
    }
    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
 Compute a checksum over core transaction data.
 */
uint256 compute_txdata_hash(const Blockchain &chain) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    for (const auto& block : chain) {
        for (const auto& tx : block) {
            auto baseSize = tx.baseSize();
            SHA256_Update(&sha256, &baseSize, sizeof(baseSize));

            auto totalSize = tx.totalSize();
            SHA256_Update(&sha256, &totalSize, sizeof(totalSize));

            auto virtualSize = tx.virtualSize();
            SHA256_Update(&sha256, &virtualSize, sizeof(virtualSize));

            auto sizeBytes = tx.sizeBytes();
            SHA256_Update(&sha256, &sizeBytes, sizeof(sizeBytes));

            auto weight = tx.weight();
            SHA256_Update(&sha256, &weight, sizeof(weight));

            auto fee = tx.fee();
            SHA256_Update(&sha256, &fee, sizeof(fee));

            auto locktime = tx.locktime();
            SHA256_Update(&sha256, &locktime, sizeof(locktime));

            auto inputCount = tx.inputCount();
            SHA256_Update(&sha256, &inputCount, sizeof(inputCount));

            auto outputCount = tx.outputCount();
            SHA256_Update(&sha256, &outputCount, sizeof(outputCount));

            auto isCoinbase = tx.isCoinbase();
            SHA256_Update(&sha256, &isCoinbase, sizeof(isCoinbase));

            auto hash = tx.getHash();
            SHA256_Update(&sha256, hash.begin(), hash.size());

            auto version = tx.getVersion();
            SHA256_Update(&sha256, &version, sizeof(version));

            auto height = tx.getBlockHeight();
            SHA256_Update(&sha256, &height, sizeof(height));

            for (const auto& input : tx.inputs()) {
                auto txIndex = input.txIndex();
                SHA256_Update(&sha256, &txIndex, sizeof(txIndex));

                auto inputIndex = input.inputIndex();
                SHA256_Update(&sha256, &inputIndex, sizeof(inputIndex));

                auto sequenceNumber = input.sequenceNumber();
                SHA256_Update(&sha256, &sequenceNumber, sizeof(sequenceNumber));

                auto age = input.age();
                SHA256_Update(&sha256, &age, sizeof(age));

                auto addrType = input.getType();
                SHA256_Update(&sha256, &addrType, sizeof(addrType));

                auto addrString = input.getAddress().toString();
                SHA256_Update(&sha256, addrString.data(), addrString.size());

                auto value = input.getValue();
                SHA256_Update(&sha256, &value, sizeof(value));

                auto spentTxIndex = input.spentTxIndex();
                SHA256_Update(&sha256, &spentTxIndex, sizeof(spentTxIndex));
            }

            for (const auto& output : tx.outputs()) {
                auto height = output.getBlockHeight();
                SHA256_Update(&sha256, &height, sizeof(height));

                auto spendingTxIndex = output.getSpendingTxIndex();
                if (spendingTxIndex) {
                    SHA256_Update(&sha256, &spendingTxIndex, sizeof(spendingTxIndex));
                }

                auto txIndex = output.txIndex();
                SHA256_Update(&sha256, &txIndex, sizeof(txIndex));

                auto outputIndex = output.outputIndex();
                SHA256_Update(&sha256, &outputIndex, sizeof(outputIndex));

                auto isSpent = output.isSpent();
                SHA256_Update(&sha256, &isSpent, sizeof(isSpent));

                auto addrType = output.getType();
                SHA256_Update(&sha256, &addrType, sizeof(addrType));

                auto addrString = output.getAddress().toString();
                SHA256_Update(&sha256, addrString.data(), addrString.size());

                auto value = output.getValue();
                SHA256_Update(&sha256, &value, sizeof(value));
            }
        }
    }
    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
 Compute a checksum over additional data (kept in separate files that are memory-mapped on-demand).
 - Input sequence numbers
 - Indexes of outputs spent by inputs
 - Transaction version
 - Transaction hashes
 - Index of first input for each transaction
 - Index of first output for each transaction
 */
uint256 compute_additional_data_hash(const DataAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    const ChainAccess &chainAccess = access.getChain();
    auto chainDirectory = access.config.chainDirectory();

    FixedSizeFileMapper<uint32_t> sequenceFile(chainAccess.sequenceFilePath(chainDirectory));
    for(uint64_t i = 0; i < chainAccess.inputCount(); ++i) {
        SHA256_Update(&sha256, sequenceFile[i], 4);
    }

    FixedSizeFileMapper<uint16_t> spentOutNumFile(chainAccess.inputSpentOutNumFilePath(chainDirectory));
    for(uint64_t i = 0; i < chainAccess.inputCount(); ++i) {
        SHA256_Update(&sha256, spentOutNumFile[i], 2);
    }

    FixedSizeFileMapper<int32_t> txVersionFile(chainAccess.txVersionFilePath(chainDirectory));
    for(uint32_t i = 0; i < chainAccess.txCount(); ++i) {
        SHA256_Update(&sha256, txVersionFile[i], 4);
    }

    FixedSizeFileMapper<uint256> txHashesFile(chainAccess.txHashesFilePath(chainDirectory));
    for(uint32_t i = 0; i < chainAccess.txCount(); ++i) {
        SHA256_Update(&sha256, txHashesFile[i], 32);
    }

    FixedSizeFileMapper<uint64_t> txFirstInputFile(chainAccess.firstInputFilePath(chainDirectory));
    for(uint32_t i = 0; i < chainAccess.txCount(); ++i) {
        SHA256_Update(&sha256, txFirstInputFile[i], 8);
    }

    FixedSizeFileMapper<uint64_t> txFirstOutputFile(chainAccess.firstOutputFilePath(chainDirectory));
    for(uint32_t i = 0; i < chainAccess.txCount(); ++i) {
        SHA256_Update(&sha256, txFirstOutputFile[i], 8);
    }

    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

template<DedupAddressType::Enum dedupType>
uint256 compute_scriptdata_hash(const DataAccess &access);

/**
 Compute a checksum over pubkey data.
 */
template<>
uint256 compute_scriptdata_hash<DedupAddressType::Enum::PUBKEY>(const DataAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    auto scripts = &access.getScripts();
    constexpr DedupAddressType::Enum dedupType = DedupAddressType::PUBKEY;
    auto scriptCount = scripts->scriptCount(dedupType);

    for(uint32_t i = 1; i <= scriptCount; ++i) {
        auto data = scripts->getScriptData<dedupType>(i);
        SHA256_Update(&sha256, &data->address, sizeof(uint160));
        SHA256_Update(&sha256, &data->pubkey, sizeof(RawPubkey));
        SHA256_Update(&sha256, &data->hasPubkey, sizeof(bool));
        SHA256_Update(&sha256, &data->txFirstSeen, 4);
        SHA256_Update(&sha256, &data->txFirstSpent, 4);
        SHA256_Update(&sha256, &data->typesSeen, 4);
    }

    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
Compute a checksum over scripthash data.
*/
template<>
uint256 compute_scriptdata_hash<DedupAddressType::Enum::SCRIPTHASH>(const DataAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    auto scripts = &access.getScripts();
    constexpr DedupAddressType::Enum dedupType = DedupAddressType::SCRIPTHASH;
    auto scriptCount = scripts->scriptCount(dedupType);

    for(uint32_t i = 1; i <= scriptCount; ++i) {
        auto data = scripts->getScriptData<dedupType>(i);
        SHA256_Update(&sha256, &data->hash160, sizeof(uint160));
        SHA256_Update(&sha256, &data->hash256, sizeof(uint256));
        SHA256_Update(&sha256, &data->isSegwit, sizeof(bool));
        SHA256_Update(&sha256, &data->txFirstSeen, 4);
        SHA256_Update(&sha256, &data->txFirstSpent, 4);
        SHA256_Update(&sha256, &data->typesSeen, 4);
        SHA256_Update(&sha256, &data->wrappedAddress, sizeof(RawAddress));
    }

    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
Compute a checksum over multisig data.
*/
template<>
uint256 compute_scriptdata_hash<DedupAddressType::Enum::MULTISIG>(const DataAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    auto scripts = &access.getScripts();
    constexpr DedupAddressType::Enum dedupType = DedupAddressType::MULTISIG;
    auto scriptCount = scripts->scriptCount(dedupType);

    for(uint32_t i = 1; i <= scriptCount; ++i) {
        auto data = scripts->getScriptData<dedupType>(i);
        SHA256_Update(&sha256, data, data->realSize());
    }

    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
Compute a checksum over OP_RETURN data.
*/
template<>
uint256 compute_scriptdata_hash<DedupAddressType::Enum::NULL_DATA>(const DataAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    auto scripts = &access.getScripts();
    constexpr DedupAddressType::Enum dedupType = DedupAddressType::NULL_DATA;
    auto scriptCount = scripts->scriptCount(dedupType);

    for(uint32_t i = 1; i <= scriptCount; ++i) {
        auto data = scripts->getScriptData<dedupType>(i);
        SHA256_Update(&sha256, data, data->realSize());
    }

    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
Compute a checksum over nonstandard data.
*/
template<>
uint256 compute_scriptdata_hash<DedupAddressType::Enum::NONSTANDARD>(const DataAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    auto scripts = &access.getScripts();
    constexpr DedupAddressType::Enum dedupType = DedupAddressType::NONSTANDARD;
    auto scriptCount = scripts->scriptCount(dedupType);

    for(uint32_t i = 1; i <= scriptCount; ++i) {
        auto data = scripts->getScriptData<dedupType>(i);
        auto script_data = std::get<0>(data);
        SHA256_Update(&sha256, script_data, script_data->realSize());
        auto spend_script_data = std::get<1>(data);
        if(spend_script_data != nullptr) {
            SHA256_Update(&sha256, spend_script_data, spend_script_data->realSize());
        }
    }

    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
Compute a checksum over unknown witness data.
*/
template<>
uint256 compute_scriptdata_hash<DedupAddressType::Enum::WITNESS_UNKNOWN>(const DataAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    auto scripts = &access.getScripts();
    constexpr DedupAddressType::Enum dedupType = DedupAddressType::WITNESS_UNKNOWN;
    auto scriptCount = scripts->scriptCount(dedupType);

    for(uint32_t i = 1; i <= scriptCount; ++i) {
        auto data = scripts->getScriptData<dedupType>(i);
        auto script_data = std::get<0>(data);
        SHA256_Update(&sha256, &script_data->witnessVersion, sizeof(script_data->witnessVersion));
        SHA256_Update(&sha256, &script_data->scriptData, script_data->scriptData.extraSize());
        auto spend_script_data = std::get<1>(data);
        if(spend_script_data != nullptr) {
            SHA256_Update(&sha256, spend_script_data, spend_script_data->realSize());
        }
    }

    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
 Compute a checksum over address range in hash index
 */
template<AddressType::Enum type>
void hash_addressrange(SHA256_CTX &sha256, const DataAccess &access) {
    auto &hashIndex = access.hashIndex;
    auto rng = hashIndex->getAddressRange<type>();
    RANGES_FOR(auto pair, rng) {
        SHA256_Update(&sha256, &pair.first, sizeof(pair.first));
        SHA256_Update(&sha256, &pair.second, sizeof(pair.second));
    }
}

/**
 Compute a checksum over all addres ranges in hash index.
 */
uint256 compute_hashindex_addressrange_hash(const DataAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    hash_addressrange<AddressType::MULTISIG>(sha256, access);
    hash_addressrange<AddressType::MULTISIG_PUBKEY>(sha256, access);
    hash_addressrange<AddressType::PUBKEY>(sha256, access);
    hash_addressrange<AddressType::PUBKEYHASH>(sha256, access);
    hash_addressrange<AddressType::SCRIPTHASH>(sha256, access);
    hash_addressrange<AddressType::WITNESS_PUBKEYHASH>(sha256, access);
    hash_addressrange<AddressType::WITNESS_SCRIPTHASH>(sha256, access);

    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

/**
 Compute a checksum over txindex lookup data in hash index.
 */
uint256 compute_hashindex_txindex_hash(const DataAccess &access) {
    uint256 hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    const ChainAccess &chainAccess = access.getChain();
    auto chainDirectory = access.config.chainDirectory();
    FixedSizeFileMapper<uint256> txHashesFile(chainAccess.txHashesFilePath(chainDirectory));
    auto &hashIndex = access.hashIndex;

    for(uint32_t i = 0; i < chainAccess.txCount(); ++i) {
        auto txindex = hashIndex->getTxIndex(*txHashesFile[i]);
        assert(*txindex == i);
        SHA256_Update(&sha256, &txindex, 4);
    }
    SHA256_Final(reinterpret_cast<unsigned char *>(&hash), &sha256);
    return hash;
}

int main(int argc, char * argv[]) {
    std::string configLocation;
    int endBlock = 0;

    auto cli = clipp::group(clipp::value("config file location", configLocation));
    auto res = parse(argc, argv, cli);
    if (res.any_error()) {
        std::cout << "Invalid command line parameter\n" << clipp::make_man_page(cli, argv[0]);
        return 0;
    }

    Blockchain chain(configLocation, endBlock);

    const DataAccess &dataAccess = chain.getAccess();
    const ChainAccess &chainAccess = dataAccess.getChain();

    std::cout << "Chain contains " << chain.size() << " blocks, " << chain.getAccess().getChain().txCount() << " txes, " << chain.getAccess().getChain().inputCount() << " inputs, " << chain.getAccess().getChain().outputCount() << " outputs." << std::endl;


    std::cout << std::endl << "Blocks (raw data):" << std::endl;
    auto rawblock_hash = compute_rawblock_hash(chainAccess);
    std::cout << rawblock_hash.GetHex() << std::endl;

    std::cout << std::endl << "Blocks (API):" << std::endl;
    auto block_hash = compute_block_hash(chain);
    std::cout << block_hash.GetHex() << std::endl;


    std::cout << std::endl << "Transactions (API):" << std::endl;
    auto txdata_hash = compute_txdata_hash(chain);
    std::cout << txdata_hash.GetHex() << std::endl;

    std::cout << std::endl << "Transactions (raw data):" << std::endl;
    auto txrawdata_hash = compute_txrawdata_hash(chainAccess);
    std::cout << txrawdata_hash.GetHex() << std::endl;


    std::cout << std::endl << "Additional data:" << std::endl;
    auto additional_data_hash = compute_additional_data_hash(dataAccess);
    std::cout << additional_data_hash.GetHex()<< std::endl;

    std::cout << std::endl << "Scripts:" << std::endl;
    blocksci::for_each(DedupAddressType::all(), [&](auto dedupType) {
        auto data_hash = compute_scriptdata_hash<dedupType>(dataAccess);
        std::cout << data_hash.GetHex() <<  " (" << dedupType << ")" << std::endl;
    });

    std::cout << std::endl << "Hash index:" << std::endl;
    auto hashindex_addressrange_hash = compute_hashindex_addressrange_hash(dataAccess);
    std::cout << hashindex_addressrange_hash.GetHex()<< std::endl;

    // TODO: convert to range query to improve performance
    // auto hashindex_txindex_hash = compute_hashindex_txindex_hash(dataAccess);
    // std::cout << hashindex_txindex_hash.GetHex()<< std::endl;

    return 0;
}
