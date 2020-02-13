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


int main(int argc, char * argv[]) {
    blocksci::BlockHeight forkHeight = 478559;

    // columns: chain, block, outputs_unspent, outputs_unspent_value, pre_fork_outputs_spent, pre_fork_outputs_spent_value, hidden_wealth, hidden_wealth_decrease
    //std::map<std::string, uint8_t> csvCols;

    //std::vector <std::array<std::string, 6>> csvData;
    //csvData.reserve(615589);

    std::ofstream hiddenWealthCsv, preForkUnspentCsv, postForkSpendPreForkOutputs;
    hiddenWealthCsv.open("/mnt/data/analysis/hidden_wealth/bch_hidden_wealth.csv");
    preForkUnspentCsv.open("/mnt/data/analysis/hidden_wealth/pre_fork_unspent.csv");
    postForkSpendPreForkOutputs.open("/mnt/data/analysis/hidden_wealth/post_fork_spend_pre_fork_outputs.csv");

    hiddenWealthCsv << "block,hidden_wealth,hidden_wealth_decrease" << std::endl;
    preForkUnspentCsv << "block,outputs_unspent,outputs_unspent_value" << std::endl;
    postForkSpendPreForkOutputs << "chain,block,pre_fork_outputs_spent,pre_fork_outputs_spent_value,current_unspent_value" << std::endl;

    std::unordered_map<blocksci::DedupAddress, uint64_t> balancesAtForkheight;
    balancesAtForkheight.reserve(400'000'000);

    blocksci::Blockchain btcForkHeight{"/mnt/data/blocksci/btc-bch/2019-12-31/btc/config.json", forkHeight};
    auto lastBlockBeforeFork = btcForkHeight[478558];
    uint32_t lastTxBeforeFork = lastBlockBeforeFork.endTxIndex() - 1;
    uint64_t unspentValueAtForkHeight = 0;

    // 1. calculate balance for all pre-fork top-level addresses on BTC
    // and calculate unspent funds at fork-height
    // iterates: PRE-FORK BTC
    for (auto block : btcForkHeight) {
        // end at fork height
        if (block.height() == forkHeight) {
            std::cout << std::endl << "block size (bytes): " << block.sizeBytes() << std::endl;
            break;
        }
        uint32_t unspentOutputsCreated = 0;
        uint64_t unspentOutputsCreatedValue = 0;

        for (auto tx : block) {
            auto outputs = tx.outputs();
            for (auto outputBtc : outputs) {
                if (outputBtc.isSpent() == false) {
                    balancesAtForkheight[{outputBtc.getAddress().scriptNum, dedupType(outputBtc.getAddress().type)}] += outputBtc.getValue();
                    unspentValueAtForkHeight += outputBtc.getValue();
                    unspentOutputsCreated++;
                    unspentOutputsCreatedValue += outputBtc.getValue();
                }
            }
            if (tx.txNum % 100000 == 0) {
                std::cout << "\r" << ((float) tx.txNum / lastTxBeforeFork) * 100 << "% (" << block.height() << " blocks done)"
                          << std::flush;
            }
        }
        preForkUnspentCsv << block.height() << "," << unspentOutputsCreated << "," << unspentOutputsCreatedValue << "\n";
    }
    preForkUnspentCsv.close();







    // 2. count how many pre-fork outputs (+value) are spent in BTC post-fork (by finding inputs that spent pre-fork outputs)
    // and track top-level addresses that are used post-fork
    // iterates: POST-FORK BTC

    std::unordered_set<blocksci::DedupAddress> btcAddrsSeenAfterFork;
    btcAddrsSeenAfterFork.reserve(400'000'000);
    uint64_t currentBtcPreForkUTXOValue = unspentValueAtForkHeight;

    blocksci::Blockchain btcCc{"/mnt/data/blocksci/btc-bch/2019-12-31/btc/config.json"};

    for (auto block : btcCc) {
        // only process after-fork blocks
        if (block.height() <= 478558) {
            continue;
        }
        uint32_t preForkOutputsSpent = 0;
        uint64_t preForkOutputsSpentValue = 0;

        for (auto tx : block) {
            auto inputs = tx.inputs();
            for (auto input : inputs) {
                auto dedupAddr = blocksci::DedupAddress{input.getAddress().scriptNum, blocksci::dedupType(input.getAddress().type)};
                btcAddrsSeenAfterFork.insert(dedupAddr);

                auto spentOutputPointer = input.getSpentOutputPointer();
                if (spentOutputPointer.txNum <= lastTxBeforeFork) {
                    // spending pre-fork output
                    preForkOutputsSpent++;
                    preForkOutputsSpentValue += input.getValue();
                    currentBtcPreForkUTXOValue -= input.getValue();
                }
            }

            auto outputs = tx.outputs();
            for(auto output: outputs) {
                auto dedupAddr = blocksci::DedupAddress{output.getAddress().scriptNum, blocksci::dedupType(output.getAddress().type)};
                btcAddrsSeenAfterFork.insert(dedupAddr);
            }
        }
        postForkSpendPreForkOutputs << "btc," << block.height() << "," << preForkOutputsSpent << "," << preForkOutputsSpentValue << "," << currentBtcPreForkUTXOValue << "\n";
    }



    blocksci::Blockchain bchCc{"/mnt/data/blocksci/btc-bch/2019-12-31/bch/config.json"};
    auto lastBlockBch = bchCc[bchCc.size() - 1];
    auto bchTxCount = lastBlockBch.endTxIndex() - 1;
    uint64_t currentBchPreForkUTXOValue = unspentValueAtForkHeight;
    uint64_t currentHiddenWealth = unspentValueAtForkHeight;
    hiddenWealthCsv << "478558," << currentHiddenWealth << ",0\n";

    // 3. count how many pre-fork outputs (+value) are spent in BCH post-fork (by finding inputs that spent pre-fork outputs)
    // and track hidden wealth
    // iterates: POST-FORK BCH
    for (auto block : bchCc) {
        // only process after-fork blocks
        if (block.height() <= 478558) {
            continue;
        }

        uint32_t preForkOutputsSpent = 0;
        uint64_t preForkOutputsSpentValue = 0;
        uint64_t hiddenWealthDecrease = 0;

        for (auto tx : block) {
            auto inputs = tx.inputs();
            for (auto input : inputs) {
                auto spentOutputPointer = input.getSpentOutputPointer();
                if (spentOutputPointer.txNum < lastTxBeforeFork) {
                    // spending pre-fork output
                    preForkOutputsSpent++;
                    preForkOutputsSpentValue += input.getValue();
                    currentBchPreForkUTXOValue -= input.getValue();
                }

                auto dedupAddr = blocksci::DedupAddress{input.getAddress().scriptNum, blocksci::dedupType(input.getAddress().type)};
                // if address occurs, subtract it's entire forkheight balance from hidden wealth
                if (btcAddrsSeenAfterFork.find(dedupAddr) != btcAddrsSeenAfterFork.end() && balancesAtForkheight.find(dedupAddr) != balancesAtForkheight.end()) {
                    hiddenWealthDecrease += balancesAtForkheight[dedupAddr];
                    balancesAtForkheight.erase(dedupAddr);
                }
            }

            auto outputs = tx.outputs();
            for (auto output : outputs) {
                auto dedupAddr = blocksci::DedupAddress{output.getAddress().scriptNum, blocksci::dedupType(output.getAddress().type)};
                // if address occurs, subtract it's entire forkheight balance from hidden wealth
                if (btcAddrsSeenAfterFork.find(dedupAddr) != btcAddrsSeenAfterFork.end() && balancesAtForkheight.find(dedupAddr) != balancesAtForkheight.end()) {
                    hiddenWealthDecrease += balancesAtForkheight[dedupAddr];
                    balancesAtForkheight.erase(dedupAddr);
                }
            }
            if (tx.txNum % 100000 == 0) {
                std::cout << "\r" << ((float) tx.txNum / bchTxCount) * 100 << "% (" << block.height() << " blocks done)" << std::flush;
            }
        }
        currentHiddenWealth -= hiddenWealthDecrease;
        hiddenWealthCsv << block.height() << "," << currentHiddenWealth << "," << hiddenWealthDecrease << "\n";
        postForkSpendPreForkOutputs << "bch," << block.height() << "," << preForkOutputsSpent << "," << preForkOutputsSpentValue << "," << currentBchPreForkUTXOValue << "\n";
    }

    hiddenWealthCsv.close();

    return 0;
}
