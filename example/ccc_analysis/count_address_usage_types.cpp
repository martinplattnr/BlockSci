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

#include <map>


int main(int argc, char * argv[]) {
    std::vector<std::string> dates = {
        // maximum of 3 can be processed in parallel with 64GB RAM
        "2017-12-31",
        "2018-06-30",
        "2018-12-31",
        //"2019-06-30",
        //"2019-12-31"
    };

    enum Status {
        BTC_PRE_FORK,              // 0
        BTC_AFTER_FORK,            // 1
        BTC_PRE_AFTER_FORK,        // 2

        BTC_BCH_PRE_AFTER_FORK,    // 3
        BTC_BCH_AFTER_FORK,        // 4

        BTC_PRE_BCH_AFTER_FORK,    // 5
        BCH_AFTER_FORK             // 6
    };

    std::ofstream resultsCsv;
    resultsCsv.open("/mnt/data/analysis/address_usage_types/results_inputs_only.csv", std::ios::out | std::ios::app);
    resultsCsv << "date,btc_pre_fork,btc_after_fork,btc_pre_after_fork,btc_bch_pre_after_fork,btc_bch_after_fork,btc_pre_bch_after_fork,bch_after_fork" << std::endl;

    blocksci::BlockHeight forkHeight = 478558;

    std::vector<std::future<void>> futures;
    std::mutex m;

    for (const auto& date : dates) {
        futures.push_back(std::async(std::launch::async, [date, &m, &resultsCsv, &forkHeight]() {

            std::cout << "### PROCESSING DATE " << date << " ###" << std::endl;

            std::unordered_map<blocksci::DedupAddress, Status> addrStatusMap;
            addrStatusMap.clear();
            addrStatusMap.reserve(600'000'000);

            { // scope for Blockchain object
                std::cout << std::endl << "# PROCESSING BTC " << date << " #" << std::endl;
                blocksci::Blockchain btcCc{"/mnt/data/blocksci/btc-bch/" + date + "/btc/config.json"};
                uint32_t txCount = btcCc.endTxIndex();

                RANGES_FOR(auto block, btcCc) {

                    RANGES_FOR(auto tx, block) {
                        if (tx.txNum % 100000 == 0) {
                            std::cout << "\r" << ((float) tx.txNum / txCount) * 100 << "%" << std::flush;
                        }


                        RANGES_FOR(auto output, tx.outputs()) {
                            auto dedupAddr = blocksci::DedupAddress{output.getAddress().scriptNum, blocksci::dedupType(output.getAddress().type)};
                            if (dedupAddr.type == blocksci::DedupAddressType::NONSTANDARD || dedupAddr.type == blocksci::DedupAddressType::NULL_DATA || dedupAddr.type == blocksci::DedupAddressType::WITNESS_UNKNOWN) {
                                continue;
                            }
                            if (block.height() <= forkHeight) {
                                // seen for first time before fork
                                addrStatusMap[dedupAddr] = Status::BTC_PRE_FORK;
                            }
                            else {
                                if (addrStatusMap.find(dedupAddr) == addrStatusMap.end()) {
                                    // seen for first time after fork
                                    addrStatusMap[dedupAddr] = Status::BTC_AFTER_FORK;
                                }
                                else {
                                    if (addrStatusMap[dedupAddr] == Status::BTC_PRE_FORK) {
                                        // seen before and after fork
                                        addrStatusMap[dedupAddr] = Status::BTC_PRE_AFTER_FORK;
                                    }
                                }
                            }
                        }


                        RANGES_FOR(auto input, tx.inputs()) {
                            auto dedupAddr = blocksci::DedupAddress{input.getAddress().scriptNum, blocksci::dedupType(input.getAddress().type)};
                            if (dedupAddr.type == blocksci::DedupAddressType::NONSTANDARD || dedupAddr.type == blocksci::DedupAddressType::NULL_DATA || dedupAddr.type == blocksci::DedupAddressType::WITNESS_UNKNOWN) {
                                continue;
                            }
                            if (block.height() <= forkHeight) {
                                // seen for first time before fork
                                addrStatusMap[dedupAddr] = Status::BTC_PRE_FORK;
                            }
                            else {
                                if (addrStatusMap.find(dedupAddr) == addrStatusMap.end()) {
                                    // seen for first time after fork
                                    addrStatusMap[dedupAddr] = Status::BTC_AFTER_FORK;
                                }
                                else {
                                    if (addrStatusMap[dedupAddr] == Status::BTC_PRE_FORK) {
                                        // seen before and after fork
                                        addrStatusMap[dedupAddr] = Status::BTC_PRE_AFTER_FORK;
                                    }
                                }
                            }
                        }

                        // progress bar
                        if (tx.txNum % 100000 == 0) {
                            std::cout << "\rProgress: " << ((float) tx.txNum / txCount) * 100 << "%" << std::flush;
                        }
                    }
                }
            }

            { // scope for Blockchain object
                std::cout << std::endl << "# PROCESSING BCH " << date << " #" << std::endl;
                blocksci::Blockchain bchCc{"/mnt/data/blocksci/btc-bch/" + date + "/bch/config.json"};
                uint32_t txCount = bchCc.endTxIndex();

                RANGES_FOR(auto block, bchCc) {
                    // skip pre-fork blocks
                    if (block.height() <= forkHeight) {
                        continue;
                    }


                    RANGES_FOR(auto tx, block) {
                        if (tx.txNum % 100000 == 0) {
                            std::cout << "\r" << ((float) tx.txNum / txCount) * 100 << "%" << std::flush;
                        }


                        RANGES_FOR(auto output, tx.outputs()) {
                            auto dedupAddr = blocksci::DedupAddress{output.getAddress().scriptNum, blocksci::dedupType(output.getAddress().type)};
                            if (dedupAddr.type == blocksci::DedupAddressType::NONSTANDARD || dedupAddr.type == blocksci::DedupAddressType::NULL_DATA || dedupAddr.type == blocksci::DedupAddressType::WITNESS_UNKNOWN) {
                                continue;
                            }

                            if (addrStatusMap.find(dedupAddr) == addrStatusMap.end()) {
                                // seen for first time after fork
                                addrStatusMap[dedupAddr] = Status::BCH_AFTER_FORK;
                            }
                            else {
                                // seen before
                                if (addrStatusMap[dedupAddr] == Status::BTC_PRE_FORK) {
                                    // seen on BTC pre-fork only
                                    addrStatusMap[dedupAddr] = Status::BTC_PRE_BCH_AFTER_FORK;
                                }
                                else if (addrStatusMap[dedupAddr] == Status::BTC_AFTER_FORK) {
                                    // seen on BTC after-fork only
                                    addrStatusMap[dedupAddr] = Status::BTC_BCH_AFTER_FORK;
                                }
                                else if (addrStatusMap[dedupAddr] == Status::BTC_PRE_AFTER_FORK) {
                                    // seen on BTC pre and after fork
                                    addrStatusMap[dedupAddr] = Status::BTC_BCH_PRE_AFTER_FORK;
                                }
                            }
                        }


                        RANGES_FOR(auto input, tx.inputs()) {
                            auto dedupAddr = blocksci::DedupAddress{input.getAddress().scriptNum, blocksci::dedupType(input.getAddress().type)};
                            if (dedupAddr.type == blocksci::DedupAddressType::NONSTANDARD || dedupAddr.type == blocksci::DedupAddressType::NULL_DATA || dedupAddr.type == blocksci::DedupAddressType::WITNESS_UNKNOWN) {
                                continue;
                            }

                            if (addrStatusMap.find(dedupAddr) == addrStatusMap.end()) {
                                // seen for first time after fork
                                addrStatusMap[dedupAddr] = Status::BCH_AFTER_FORK;
                            }
                            else {
                                // seen before
                                if (addrStatusMap[dedupAddr] == Status::BTC_PRE_FORK) {
                                    // seen on BTC pre-fork only
                                    addrStatusMap[dedupAddr] = Status::BTC_PRE_BCH_AFTER_FORK;
                                }
                                else if (addrStatusMap[dedupAddr] == Status::BTC_AFTER_FORK) {
                                    // seen on BTC after-fork only
                                    addrStatusMap[dedupAddr] = Status::BTC_BCH_AFTER_FORK;
                                }
                                else if (addrStatusMap[dedupAddr] == Status::BTC_PRE_AFTER_FORK) {
                                    // seen on BTC pre and after fork
                                    addrStatusMap[dedupAddr] = Status::BTC_BCH_PRE_AFTER_FORK;
                                }
                            }
                        }

                        // progress bar
                        if (tx.txNum % 100000 == 0) {
                            std::cout << "\rProgress: " << ((float) tx.txNum / txCount) * 100 << "%" << std::flush;
                        }
                    }
                }
            }

            std::vector<uint32_t> results(7, 0);
            for (const auto& mapEntry : addrStatusMap) {
                results[mapEntry.second]++;
            }

            std::cout << std::endl;
            {
                std::lock_guard<std::mutex> lock(m);
                resultsCsv << date << "," << results[0] << "," << results[1] << "," << results[2] << "," << results[3] << "," << results[4] << "," << results[5] << "," << results[6] << std::endl;
            }

        }));
    }

    for (auto &future : futures) {
        future.get();
    }

    resultsCsv.close();

    return 0;
}
