#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>
#include <internal/address_info.hpp>

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>


int main(int argc, char * argv[]) {
    blocksci::Blockchain btcCcT0{"/mnt/data/blocksci/btc-bch/2017-12-31/btc/config.json"};

    auto cccT0Clustering = blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc-bch/btc/2017-12-31", btcCcT0.getAccess());

    std::vector<blocksci::ClusterManager*> clusterings = {
        new blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc/2017-12-31", btcCcT0.getAccess()), // 2017-12-30
        new blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc/2018-06-30", btcCcT0.getAccess()),
        new blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc/2018-12-31", btcCcT0.getAccess()),
        new blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc/2019-06-30", btcCcT0.getAccess()),
        new blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc/2019-12-31", btcCcT0.getAccess()),
    };

    std::fstream fout_results, fout_merge_counts;
    fout_results.open("/mnt/data/analysis/ccc_verfication_results_.csv", std::ios::out);
    fout_merge_counts.open("/mnt/data/analysis/ccc_verfication_results_merge_counts_.csv", std::ios::out);

    // write CSV file headers
    fout_results
        << "clusterId,"
        << "clusterSize";
    for (uint32_t i = 0; i < clusterings.size(); i++) {
        fout_results << ",clustersInBtcT" << i;
        fout_results << ",mergesInBtcT" << i;
    }
    fout_results << std::endl;

    fout_merge_counts << "chain,reference,period,mergeCount" << std::endl;

    std::vector <uint32_t> btcMergesPerPeriod(clusterings.size(), 0);

    std::cout << std::setprecision(3);

    /* investigate huge ccc merges that were not present in the 595303 parse
     * 52663,1341466,328072,-,326448,1624,318358,8090,318343,15,318340,3
     * 63684,3355270,589490,-,586737,2753,586247,490,585855,392,585830,25
     * 70173,2979468,1250868,-,934267,316601,703138,231129,702981,157,702958,23
     */

    std::cout << "Counting merge events between single-chain BTC clusterings: " << std::endl;
    std::vector<std::future<uint32_t>> futures(clusterings.size());
    for (uint32_t i = 1; i < clusterings.size(); i++) {
        futures[i-1] = std::async(std::launch::async,
            [&clusterings, i]() -> uint32_t {
                uint32_t btcMergesPerPeriod = 0;
                uint32_t newAddressCount = 0;
                RANGES_FOR (auto btcCurrentPeriodCluster, clusterings[i]->getClusters()) {
                    uint32_t clusterSize = btcCurrentPeriodCluster.getTypeEquivSize();

                    if (btcCurrentPeriodCluster.clusterNum % 1000000 == 0 && i == clusterings.size() - 1) {
                        std::cout << "\r" << ((float) btcCurrentPeriodCluster.clusterNum / clusterings[i]->getClusterCount()) * 100 << "%" << std::flush;
                    }

                    if (clusterSize == 1) {
                        auto clusterInPrevBtcPeriod = clusterings[i-1]->getCluster(blocksci::RawAddress{btcCurrentPeriodCluster.getDedupAddresses()[0].scriptNum, reprType(btcCurrentPeriodCluster.getDedupAddresses()[0].type)});
                        if (! clusterInPrevBtcPeriod) {
                            newAddressCount++;
                        }
                        continue;
                    }

                    std::unordered_set<uint32_t> clustersInPrevBtcPeriod;
                    auto btcCurrentPeriodClusterDedupAddrs = btcCurrentPeriodCluster.getDedupAddresses();
                    // does currently not include merges of new addresses
                    for (auto btcCurrentPeriodAddr : btcCurrentPeriodClusterDedupAddrs) {
                        auto clusterInPrevBtcPeriod = clusterings[i-1]->getCluster(blocksci::RawAddress{btcCurrentPeriodAddr.scriptNum, reprType(btcCurrentPeriodAddr.type)});
                        if (clusterInPrevBtcPeriod) {
                            if (clustersInPrevBtcPeriod.find(clusterInPrevBtcPeriod->clusterNum) == clustersInPrevBtcPeriod.end()) {
                                clustersInPrevBtcPeriod.insert(clusterInPrevBtcPeriod->clusterNum);
                            }
                        }
                        else {
                            // new address that does not exist in previous clustering
                            btcMergesPerPeriod++;
                            newAddressCount++;
                        }
                    }

                    uint32_t componentsInPrevBtcPeriod = clustersInPrevBtcPeriod.size();
                    if (componentsInPrevBtcPeriod > 1) {
                        btcMergesPerPeriod += componentsInPrevBtcPeriod - 1;
                    }
                }
                std::cout << i << ",newAddressCount=" << newAddressCount << std::endl;
                return btcMergesPerPeriod;
            }
        );
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    for (uint32_t i = 1; i < clusterings.size(); i++) {
        btcMergesPerPeriod[i] = futures[i-1].get();
        fout_merge_counts << "btc,btc," << i << "," << btcMergesPerPeriod[i] << std::endl;
        std::cout << "Merge events between BTC T" << i-1 << "and T" << i << ":" << btcMergesPerPeriod[i] << std::endl;
    }

    uint32_t userWhereNotAllMergesAreFoundUntilTn = 0;
    uint32_t ccClustersWithScClusters = 0;
    uint32_t cccMergeCount = 0;
    std::vector<uint32_t> cccMergesInBtcCount(clusterings.size(), 0);

    RANGES_FOR (auto cccCluster, cccT0Clustering.getClusters()) {
        uint32_t cccClusterSize = cccCluster.getTypeEquivSize();

        if (cccCluster.clusterNum % 500000 == 0) {
            std::cout << "\rProgress: " << ((float) cccCluster.clusterNum / cccT0Clustering.getClusterCount()) * 100 << "%" << std::flush;
        }

        if (cccClusterSize == 1) {
            continue;
        }

        std::vector<std::unordered_set<uint32_t>> clustersInBtc(clusterings.size());
        auto cccClusterDedupAddrs = cccCluster.getDedupAddresses();

        for (auto cccDedupAddr : cccClusterDedupAddrs) {
            for (uint32_t i = 0; i < clusterings.size(); i++) {
                auto clusterInBtc = clusterings[i]->getCluster(blocksci::RawAddress{cccDedupAddr.scriptNum, reprType(cccDedupAddr.type)});
                if (clustersInBtc[i].find(clusterInBtc->clusterNum) == clustersInBtc[i].end()) {
                    clustersInBtc[i].insert(clusterInBtc->clusterNum);
                }
            }
        }
        if (clustersInBtc[0].size() > 1) {
            ccClustersWithScClusters++;
        }

        if (clustersInBtc[0].size() > 1 && clustersInBtc[clusterings.size()-1].size() > 1) {
            // userWhereNotAllMergesAreFoundUntilT4=99559
            userWhereNotAllMergesAreFoundUntilTn++;
        }

        cccMergeCount += clustersInBtc[0].size() - 1;

        bool relevant = false;
        for (uint32_t i = 1; i < clusterings.size(); i++) {
            if (clustersInBtc[i-1].size() != clustersInBtc[i].size()) {
                relevant = true;
                break;
            }
        }

        if (relevant) {
            fout_results
                << cccCluster.clusterNum << ","
                << cccClusterSize;

            for (uint32_t i = 0; i < clusterings.size(); i++) {
                fout_results << "," << clustersInBtc[i].size();
                if (i > 0) {
                    uint32_t cccMergesInBtcInPeriodI = clustersInBtc[i-1].size() - clustersInBtc[i].size();
                    cccMergesInBtcCount[i] += cccMergesInBtcInPeriodI;
                    fout_results << "," << cccMergesInBtcInPeriodI;
                }
                else {
                    fout_results << ",-";
                }
            }

            fout_results << std::endl;
        }
    }

    for (uint32_t i = 0; i < clusterings.size(); i++) {
        fout_merge_counts << "btc,ccc," << i << "," << cccMergesInBtcCount[i] << std::endl;
    }

    fout_merge_counts << "ccc,btc,0," << cccMergeCount << std::endl;

    std::cout << "userWhereNotAllMergesAreFoundUntilTn=" << userWhereNotAllMergesAreFoundUntilTn << std::endl;
    std::cout << "ccClustersWithScClusters=" << ccClustersWithScClusters << std::endl;

    return 0;
}