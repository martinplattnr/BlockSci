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

    uint32_t forkHeight = 478559;
    uint32_t spentOnBtcNotBchCount = 0;
    uint64_t hiddenWealthSatoshis = 0;
    std::unordered_set<blocksci::DedupAddress> affectedAddrs;
    affectedAddrs.reserve(10'000'000);

    for (auto block : btcCc) {
        if (block.height() == forkHeight) {
            std::cout << "block size (bytes): " << block.sizeBytes() << std::endl;
            break;
        }

        for (auto tx : block) {
            auto outputs = tx.outputs();
            for (auto outputBtc : outputs) {
                blocksci::OutputPointer pointerBch = outputBtc.pointer;
                pointerBch.chainId = blocksci::ChainId::BITCOIN_CASH;
                auto outputBch = blocksci::Output{pointerBch, bchCc.getAccess()};

                // sanity check
                if (outputBtc.getAddress().scriptNum != outputBch.getAddress().scriptNum
                    || outputBtc.getAddress().type != outputBch.getAddress().type
                    || outputBtc.getValue() != outputBch.getValue()) {
                    throw std::runtime_error("something is wrong");
                }

                if (outputBtc.isSpent() && !outputBch.isSpent()) {
                    affectedAddrs.insert({outputBtc.getAddress().scriptNum, blocksci::dedupType(outputBtc.getAddress().type)});
                    hiddenWealthSatoshis += outputBtc.getValue();
                    spentOnBtcNotBchCount++;
                }
            }
        }
    }

    std::cout << "spentOnBtcNotBchCount=" << spentOnBtcNotBchCount << std::endl;
    std::cout << "hiddenWealthSatoshis=" << hiddenWealthSatoshis << std::endl;
    std::cout << "hiddenWealthBtc=" << hiddenWealthSatoshis / 100000000 << std::endl;
    std::cout << "affectedAddresses.size()=" << affectedAddrs.size() << std::endl;

    return 0;
}
