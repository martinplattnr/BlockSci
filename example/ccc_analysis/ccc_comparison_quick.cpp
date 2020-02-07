//
// Created by martin on 05.02.20.
//

#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <thread>


int main(int argc, char * argv[]) {
    std::vector<std::string> dates = {
        "2017-12-31",
        "2018-06-30",
        "2018-12-31",
        "2019-06-30",
        "2019-12-31"
    };

    std::fstream fout_ccc_comparison_quick_combined;
    fout_ccc_comparison_quick_combined.open("/mnt/data/analysis/ccc_comparison_quick_combined.csv", std::ios::out);
    fout_ccc_comparison_quick_combined
            << "date,"
            << "clusterId,"
            << "clusterSize,"
            << "clusterCountInSc,"
            << "merges"
            << std::endl;

    std::vector<std::future<void>> futures;
    std::mutex m;
    uint32_t i = 0;
    uint32_t lastDateIndex = dates.size() - 1;
    for (const auto &date : dates) {
        futures.push_back(std::async(std::launch::async, [date, &m, i, &lastDateIndex, &fout_ccc_comparison_quick_combined]() {
            blocksci::Blockchain btc {"/mnt/data/blocksci/btc-bch/" + date + "/btc/config.json"};
            auto ccClustering = blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc-bch/btc/" + date, btc.getAccess());
            auto scClustering = blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc/" + date, btc.getAccess());

            uint32_t ccClusterCount = ccClustering.getClusterCount();

            std::fstream fout_ccc_comparison_quick_single;
            fout_ccc_comparison_quick_single.open("/mnt/data/analysis/ccc_comparison_quick_" + date + ".csv", std::ios::out);

            // write CSV file headers
            fout_ccc_comparison_quick_single
                << "clusterId,"
                << "clusterSize,"
                << "clusterCountInSc,"
                << "merges"
                << std::endl;

            uint32_t ccMerges = 0;
            uint32_t affectedAddresses = 0;
            RANGES_FOR (auto ccCluster, ccClustering.getClusters()) {
                if (i == lastDateIndex && ccCluster.clusterNum % 1000000 == 0) {
                    std::cout << "\rProgress: " << ((float) ccCluster.clusterNum / ccClusterCount) * 100 << "%" << std::flush;
                }

                uint32_t ccClusterSize = ccCluster.getTypeEquivSize();
                if (ccClusterSize == 0) {
                    std::cout << "error: ccClusterSize==0" << std::endl << std::flush;
                    throw std::runtime_error("empty cluster");
                }
                if (ccClusterSize == 1) {
                    continue;
                }

                auto ccDedupAddrs = ccCluster.getDedupAddresses();
                std::unordered_set<uint32_t> clustersInSc;
                for (auto ccDedupAddr : ccDedupAddrs) {
                    // find the cluster in the single-chain clustering that contains the current address
                    auto scCluster = scClustering.getCluster(blocksci::RawAddress{ccDedupAddr.scriptNum, reprType(ccDedupAddr.type)});
                    if (scCluster) {
                        if (clustersInSc.find(scCluster->clusterNum) == clustersInSc.end()) {
                            clustersInSc.insert(scCluster->clusterNum);
                        }
                    }
                    else {
                        std::cout << "error: address not found in SC-clustering" << std::endl << std::flush;
                        throw std::runtime_error("address not found in SC-clustering");
                    }
                }

                if (clustersInSc.size() > 1) {
                    affectedAddresses += ccClusterSize;
                    ccMerges += clustersInSc.size() - 1;

                    fout_ccc_comparison_quick_single
                        << ccCluster.clusterNum << ","
                        << ccClusterSize << ","
                        << clustersInSc.size() << ","
                        << clustersInSc.size() - 1
                        << std::endl;

                    std::lock_guard<std::mutex> lock(m);
                    fout_ccc_comparison_quick_combined
                        << date << ","
                        << ccCluster.clusterNum << ","
                        << ccClusterSize << ","
                        << clustersInSc.size() << ","
                        << clustersInSc.size() - 1
                        << std::endl;
                }
            }

            {
                std::lock_guard<std::mutex> lock(m);
                std::cout << "### COMPARED DATE " << date << " ###" << std::endl;
                std::cout << "ccClustering.getClusterCount()=" << ccClusterCount << std::endl;
                std::cout << "scClustering.getClusterCount()=" << scClustering.getClusterCount() << std::endl << std::endl;
                std::cout << (scClustering.getClusterCount() - ccClusterCount) << " merges (calculated) found" << std::endl;
                std::cout << ccMerges << " merges (counted) found" << std::endl;
                std::cout << affectedAddresses << " affected addresses" << std::endl << std::endl << std::flush;
            }

            fout_ccc_comparison_quick_single.close();
        }));
        i++;
    }

    for (auto &future : futures) {
        future.get();
    }

    fout_ccc_comparison_quick_combined.close();

    return 0;
}
