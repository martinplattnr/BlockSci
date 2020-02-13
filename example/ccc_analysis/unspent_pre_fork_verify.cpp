//
// Created by martin on 13.02.20.
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
#include <map>
#include <mutex>


int main(int argc, char * argv[]) {
    std::ofstream preForkSpendingsCsv;
    preForkSpendingsCsv.open("/mnt/data/analysis/untouched_forked/bch_pre_fork_spendings_verify.csv");
    preForkSpendingsCsv << "chain,block,unspentvalue" << std::endl;

    blocksci::Blockchain btcCc{"/mnt/data/blocksci/btc-bch/2019-12-31/btc/config.json"};
    blocksci::Blockchain bchCc{"/mnt/data/blocksci/btc-bch/2019-12-31/bch/config.json"};
    blocksci::BlockHeight forkHeight = 478559;

    auto lastBlockBeforeFork = btcCc[478558];
    uint32_t lastTxBeforeFork = lastBlockBeforeFork.endTxIndex() - 1;

    uint64_t unspentOfForkUtxos = 1648197729530316;
    RANGES_FOR (auto block, btcCc) {
        if (block.height() <= 478558) {
            continue;
        }
        RANGES_FOR (auto tx, block) {
            auto inputs = tx.inputs();
            uint32_t preForkSpends = 0;
            uint64_t preForkSpendsValue = 0;
            RANGES_FOR (auto input, inputs) {
                auto spentOutputPointer = input.getSpentOutputPointer();
                if (spentOutputPointer.txNum <= lastTxBeforeFork) {
                    unspentOfForkUtxos -= input.getValue();
                }
            }
        }
        preForkSpendingsCsv << "btc," << block.height() << "," << unspentOfForkUtxos << "\n";
    }

    unspentOfForkUtxos = 1648197729530316;
    RANGES_FOR (auto block, bchCc) {
        if (block.height() <= 478558) {
            continue;
        }
        RANGES_FOR (auto tx, block) {
            auto inputs = tx.inputs();
            uint32_t preForkSpends = 0;
            uint64_t preForkSpendsValue = 0;
            RANGES_FOR (auto input, inputs) {
                auto spentOutputPointer = input.getSpentOutputPointer();
                if (spentOutputPointer.txNum <= lastTxBeforeFork) {
                    unspentOfForkUtxos -= input.getValue();
                }
            }
        }
        preForkSpendingsCsv << "bch," << block.height() << "," << unspentOfForkUtxos << "\n";
    }
    preForkSpendingsCsv.close();

    return 0;
}
