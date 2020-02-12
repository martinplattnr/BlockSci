//
// Created by martin on 12.02.20.
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
    // TODO: this code is incomplete!

    blocksci::Blockchain bchCc{"/mnt/data/blocksci/btc-bch/2019-12-31/bch/config.json"};

    uint32_t dust = 2000; // ~ € 0,20 as of 2020-02-12

    auto lastBlockBeforeFork = bchCc[478558];
    uint32_t lastTxIndexBeforeFork = lastBlockBeforeFork.endTxIndex() - 1;

    for (auto block : bchCc) {
        if (block.height() < 478559) {
            continue;
        }

        for (auto tx : block) {
            // ignore coinjoin
            if (blocksci::heuristics::isCoinjoin(tx)) {
                continue;
            }

            auto inputs = tx.inputs();

            // check that all inputs spent pre-fork outputs
            for (auto input : inputs) {
                if (input.spentTxIndex() > lastTxIndexBeforeFork) {
                    continue;
                }
            }

            uint32_t inputBalance = 0;
            std::unordered_set<blocksci::DedupAddress> inputAddrs;
            for (auto input : inputs) {
                auto addr = input.getAddress();
                // calculate and add balance only once per address
                if (inputAddrs.find({addr.scriptNum, blocksci::dedupType(addr.type)}) == inputAddrs.end()) {
                    // todo: use eqivaddr to calculate balance
                    inputBalance += input.getAddress().calculateBalance(block.height() + 1); // calculate for the next block
                    inputAddrs.insert({addr.scriptNum, blocksci::dedupType(addr.type)});
                }
            }
            if (inputBalance < dust && tx.outputCount() == 1) {
                // might be cashout
            }
        }
    }

    return 0;
}
