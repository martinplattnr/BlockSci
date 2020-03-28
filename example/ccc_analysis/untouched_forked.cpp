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
#include <map>
#include <mutex>


int main(int argc, char * argv[]) {

    blocksci::Blockchain chain1("/mnt/data/blocksci/btc/periods/2019-12-31/config.json");
    blocksci::Blockchain chain2("/mnt/data/blocksci/btc-bch/2019-12-31/btc/config.json");
    auto &access1 = chain1.getAccess();
    auto &access2 = chain2.getAccess();
    auto scripts1 = &access1.getScripts();
    auto scripts2 = &access2.getScripts();
    blocksci::DedupAddressType::Enum dedupType = blocksci::DedupAddressType::PUBKEY;
    auto scriptCount = scripts1->scriptCount(dedupType);
    for(uint32_t i = 1; i <= scriptCount; ++i) {
        auto data1 = scripts1->getScriptHeader(i, dedupType);
        auto data2 = scripts2->getScriptHeader(i, dedupType);
        if(data1->txFirstSpent != data2->txFirstSpent) {
            std::cout << i << std::endl;
        }
    }

    return 0;


    std::vector<std::pair<uint32_t, uint64_t>> whenSpent;
    whenSpent.reserve(615796);

    const size_t bufsize = 1024 * 1024 * 128;
    std::unique_ptr<char[]> buf1(new char[bufsize]);
    std::unique_ptr<char[]> buf2(new char[bufsize]);

    std::ofstream preForkCsv, postForkCsv;
    preForkCsv.rdbuf()->pubsetbuf(buf1.get(), bufsize);
    postForkCsv.rdbuf()->pubsetbuf(buf2.get(), bufsize);

    preForkCsv.open("/mnt/data/analysis/untouched_forked/bch_pre_fork_output_stats_creation.csv");
    postForkCsv.open("/mnt/data/analysis/untouched_forked/bch_pre_fork_output_stats_spending.csv");

    preForkCsv << "block,outputs_created,outputs_created_value,outputs_spent,outputs_spent_value" << std::endl;
    postForkCsv << "block,pre_fork_outputs_spent,pre_fork_outputs_spent_value" << std::endl;

    blocksci::Blockchain bchCc{"/mnt/data/blocksci/btc-bch/2019-12-31/bch/config.json"};
    blocksci::BlockHeight forkHeight = 478559;

    std::mutex m;

    blocksci::BlockRange preForkBlocks = bchCc[{0, forkHeight}];

    // check for correct fork height
    /*
    std::function<uint32_t(const blocksci::Block&)> mapFunc2 = [&](const blocksci::Block& block) -> uint32_t {
        if (block.height() > forkHeight - 3) {
            std::cout << block.height() << "," << block.sizeBytes() << std::endl;
        }
    };
    preForkBlocks.map(mapFunc2);
    */
    uint32_t txCount = bchCc[478559].endTxIndex();
    std::atomic<uint32_t> processedTxes;
    processedTxes = 0;
    std::atomic<uint32_t> processedBlocks;
    processedBlocks = 0;

    std::function<uint32_t(const blocksci::Block&)> mapFunc = [&processedTxes, &processedBlocks, &m, &txCount, &whenSpent, &preForkCsv] (const blocksci::Block& block) -> uint32_t {
        uint32_t outputsCreated = 0;
        uint64_t outputsCreatedValue = 0;
        uint32_t outputsSpent = 0;
        uint64_t spentValue = 0;

        if (processedTxes > 0 && processedTxes % 500 == 0) {
            std::cout << "\r" << ((float) processedTxes / txCount) * 100 << "% (" << processedBlocks << " blocks done)" << std::flush;
        }

        RANGES_FOR (auto tx, block) {
            auto outputs = tx.outputs();
            RANGES_FOR (auto output, outputs) {
                uint64_t outputValue = output.getValue();
                outputsCreated++;
                outputsCreatedValue += outputValue;
                if (output.isSpent()) {
                    outputsSpent++;
                    spentValue += outputValue;

                    auto spendingHeight = output.getSpendingTx()->getBlockHeight();
                    std::lock_guard<std::mutex> lock(m);
                    whenSpent[spendingHeight].first++;
                    whenSpent[spendingHeight].second += outputValue;
                }
            }
            ++processedTxes;
        }
        ++processedBlocks;

        {
            std::lock_guard<std::mutex> lock(m);
            preForkCsv << block.height() << "," << outputsCreated << "," << outputsCreatedValue << "," << outputsSpent << "," << spentValue << "\n";
        }

        return 0;
    };

    preForkBlocks.map(mapFunc);

    for (std::size_t block = 0; block != whenSpent.size(); ++block) {
        postForkCsv << block << "," << whenSpent[block].first << "," << whenSpent[block].second << "\n";
    }

    preForkCsv.close();
    postForkCsv.close();

    return 0;

    /*
    RANGES_FOR (auto block, bchCc) {
        if (block.height() == forkHeight) {
            std::cout << "block size (bytes): " << block.sizeBytes() << std::endl;
            break;
        }

        uint32_t outputsCreated = 0;
        uint64_t outputsCreatedValue = 0;
        uint32_t outputsSpent = 0;
        uint64_t spentValue = 0;

        RANGES_FOR (auto tx, block) {
            auto outputs = tx.outputs();
            RANGES_FOR (auto output, outputs) {
                uint64_t outputValue = output.getValue();
                outputsCreated++;
                outputsCreatedValue += outputValue;
                if (output.isSpent()) {
                    outputsSpent++;
                    spentValue += outputValue;

                    auto spendingHeight = output.getSpendingTx()->getBlockHeight();
                    whenSpent[spendingHeight].first++;
                    whenSpent[spendingHeight].second += outputValue;
                }
            }
            if (tx.txNum % 10000 == 0) {
                std::cout << "\r" << ((float) tx.txNum / txCount) * 100 << "%" << std::flush;
            }
        }
        preForkCsv << block.height() << "," << outputsCreated << "," << outputsCreatedValue << "," << outputsSpent << "," << spentValue << "\n";
    }
    */

    for (std::size_t block = 0; block != whenSpent.size(); ++block) {
        postForkCsv << block << "," << whenSpent[block].first << "," << whenSpent[block].second << "\n";
    }

    preForkCsv.close();
    postForkCsv.close();

    return 0;

    /* previous version
    std::map<std::string, blocksci::BlockHeight> dateHeightMap = {
        {"2017-12-31", 510930},
        {"2018-06-30", 536977},
        {"2018-12-31", 563309},
        {"2019-06-30", 589330},
        {"2019-12-31", 615795}
    };

    for (const auto& dateHeightPair : dateHeightMap) {
        auto& date = dateHeightPair.first;
        auto& height = dateHeightPair.second;

        blocksci::Blockchain bchCc{"/mnt/data/blocksci/btc-bch/2019-12-31/bch/config.json", height};

        std::cout << "Checking date " << date << std::endl;
        uint32_t totalOutputs = 0;
        uint64_t totalOutputsValue = 0;
        uint32_t spentOutputs = 0;
        uint64_t spentOutputsValue = 0;

        for (auto block : bchCc) {
            if (block.height() == forkHeight) {
                std::cout << "block size (bytes): " << block.sizeBytes() << std::endl;
                break;
            }

            for (auto tx : block) {
                auto outputs = tx.outputs();
                for (auto output : outputs) {
                    totalOutputs++;
                    totalOutputsValue += output.getValue();
                    if (output.isSpent()) {
                        spentOutputs++;
                        spentOutputsValue += output.getValue();
                    }
                }
            }
        }

        std::cout << "totalOutputs=" << totalOutputs << std::endl;
        std::cout << "totalOutputsValue=" << totalOutputsValue << std::endl;
        std::cout << "spentOutputs=" << spentOutputs << std::endl;
        std::cout << "spentOutputsValue=" << spentOutputsValue << std::endl;
    }

    return 0;
    */
}
