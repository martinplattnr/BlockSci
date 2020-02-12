//
// Created by martin on 11.02.20.
//


#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>
#include <internal/chain_access.hpp>
#include <internal/hash_index.hpp>
#include <internal/address_info.hpp>
#include <internal/dedup_address_info.hpp>

#include <iostream>
#include <chrono>

#include <mpark/variant.hpp>


int main(int argc, char * argv[]) {
    blocksci::Blockchain btcCc{"/mnt/data/blocksci/btc-bch/2019-12-31/btc/config.json"};
    blocksci::Blockchain bchCc{"/mnt/data/blocksci/btc-bch/2019-12-31/bch/config.json"};

    auto lastBlockBeforeFork = btcCc[478558];
    uint32_t lastTxIndexBeforeFork = lastBlockBeforeFork.endTxIndex() - 1;

    std::unordered_map<blocksci::DedupAddressType::Enum, uint32_t> commonAddrs = {
        {blocksci::DedupAddressType::SCRIPTHASH, 0},
        {blocksci::DedupAddressType::PUBKEY, 0},
        //{blocksci::DedupAddressType::MULTISIG, 0}, // for multisigs, it should be checked if they are toplevel addresses (?), and only then check
    };


    for (auto& pair : commonAddrs) {
        auto& dedupType = pair.first;
        auto& commonCount = pair.second;

        // todo: check if this does not over-count, as it may check bch's scripts against itself
        auto scriptCount = btcCc.getAccess().scripts->scriptCount(dedupType);
        std::cout << std::endl << "comparing " << scriptCount << blocksci::dedupAddressName(dedupType) << " addresses:" << std::endl;

        for (uint32_t i = 1; i <= scriptCount; ++i) {
            auto scriptInBtc = blocksci::Address{i, blocksci::reprType(dedupType), btcCc.getAccess()}.getBaseScript();
            auto firstTxIndexBtc = scriptInBtc.getFirstTxIndex();
            /* result without the ! firstTxIndexBtc (eg. "does exist on BTC"-check):
             * comparing 700183225pubkey_script addresses:
                99.9881% (Current common count: 19990464) <-- why is this not higher (43272428 are new on btc)
                comparing 150980605scripthash_script addresses:
                99.9466% (Current common count: 1585777)
                Process finished with exit code 0
            */
            if ( ! firstTxIndexBtc || (firstTxIndexBtc && *firstTxIndexBtc <= lastTxIndexBeforeFork)) {
            //if ((firstTxIndexBtc && *firstTxIndexBtc <= lastTxIndexBeforeFork)) {
                // address existed before fork
                continue;
            }

            auto scriptInBch = blocksci::Address{i, blocksci::reprType(dedupType), bchCc.getAccess()}.getBaseScript();
            auto firstTxIndexBch = scriptInBch.getFirstTxIndex();
            // !firstTxIndexBtc just here to test
            //if (!firstTxIndexBtc && firstTxIndexBch && *firstTxIndexBch > lastTxIndexBeforeFork) {
            if (firstTxIndexBch && *firstTxIndexBch > lastTxIndexBeforeFork) {
                ++commonCount;
                //std::cout << scriptInBtc.toString() << std::endl;
            }
            if (i % 100000 == 0) {
                std::cout << "\r" << ((float) i / scriptCount) * 100 << "% (Current common count: " << commonCount << ")"
                          << std::flush;
            }
        }
    }

    /*
    // SCRIPTHASH ADDRESSES (incl. witness script hash)
    uint32_t commonScriptHashAddresses = 0;
    auto dedupType = blocksci::DedupAddressType::Enum::SCRIPTHASH;
    auto scriptCount = btcCc.getAccess().scripts->scriptCount(dedupType);
    std::cout << std::endl << "comparing " << scriptCount << "scripthash addresses:" << std::endl;
    for (uint32_t i = 1; i <= scriptCount; ++i) {
        auto scriptHashInBtc = blocksci::script::ScriptHash(i, btcCc.getAccess());
        auto firstTxIndexBtc = scriptHashInBtc.getFirstTxIndex();
        if (firstTxIndexBtc && *firstTxIndexBtc <= lastTxIndexBeforeFork) {
            // address existed before fork
            continue;
        }

        auto scriptHashInBch = blocksci::script::ScriptHash(i, bchCc.getAccess());
        auto firstTxIndexBch = scriptHashInBch.getFirstTxIndex();
        if (firstTxIndexBch && *firstTxIndexBch > lastTxIndexBeforeFork) {
            ++commonScriptHashAddresses;
            std::cout << scriptHashInBtc.toString() << std::endl;
            break;
        }
        if (i % 10000 == 0) {
            std::cout << "\r" << ((float) i / scriptCount) * 100 << "% (Result: " << commonScriptHashAddresses << ")"
                      << std::flush;
        }
    }
    std::cout << "commonScriptHashAddresses=" << commonScriptHashAddresses << std::endl << std::endl << std::endl;

    // PUBKEY ADDRESSES (all types: P2PKH, P2PK, witness/non-witness)
    uint32_t commonPubkeyAddresses = 0;
    dedupType = blocksci::DedupAddressType::Enum::PUBKEY;
    scriptCount = btcCc.getAccess().scripts->scriptCount(dedupType);
    std::cout << std::endl << "comparing " << scriptCount << " pubkey addresses:" << std::endl;
    for (uint32_t i = 1; i <= scriptCount; ++i) {
        auto pubkeyInBtc = blocksci::script::PubkeyHash (i, btcCc.getAccess());
        auto firstTxIndex = pubkeyInBtc.getFirstTxIndex();
        if (firstTxIndex && *firstTxIndex <= lastTxIndexBeforeFork) {
            // address existed before fork
            continue;
        }

        auto pubkeyInBch = blocksci::script::PubkeyHash(i, bchCc.getAccess());
        auto firstTxIndexBch = pubkeyInBch.getFirstTxIndex();
        if (firstTxIndexBch && *firstTxIndexBch > lastTxIndexBeforeFork) {
            ++commonScriptHashAddresses;
            std::cout << pubkeyInBtc.toString() << std::endl;
        }

        if (i % 10000 == 0) {
            std::cout << "\r" << ((float) i / scriptCount) * 100 << "% (Result: " << commonPubkeyAddresses << ")"
                      << std::flush;
        }
    }
    std::cout << "commonPubkeyAddresses=" << commonPubkeyAddresses << std::endl << std::endl << std::endl;
    */

    return 0;
}
