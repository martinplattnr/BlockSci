#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>
#include <internal/address_info.hpp>

#include <iostream>
#include <string>


int main(int argc, char * argv[]) {
    blocksci::Blockchain btc{"/mnt/data/blocksci/bitcoin/595303-root-v0.6-0e6e863/config.json"};

    auto cccClusteringT0 = blocksci::ClusterManager(
            "/mnt/data/blocksci/ccc_python_btc_bch_reduceto_btc_with_log__534602", btc.getAccess());
    uint32_t cccClusteringCount = cccClusteringT0.getClusterCount();
    auto btcClusteringT0 = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc_534602", btc.getAccess());

    auto btcClusteringT1 = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc_587985", btc.getAccess());
    uint32_t btcT1ClusteringCount = cccClusteringT0.getClusterCount();

    std::fstream fout_results;
    fout_results.open("/mnt/data/analysis/ccc_verfication_results.csv", std::ios::out | std::ios::app);

    std::stringstream results_headers;

    // write CSV file headers
    results_headers
        << "clusterId,"
        << "clusterSize,"
        << "clustersInBtcT0,"
        << "clustersInBtcT1,"
        << "\n";
    fout_results << results_headers.str();

    uint32_t mergesBtcT1vsT0 = 0;


    RANGES_FOR (auto btcT1Cluster, btcClusteringT1.getClusters()) {
        std::unordered_set<uint32_t> clustersInBtcT0;
        auto btcT1ClusterDedupAddrs = btcT1Cluster.getDedupAddresses();

        if (btcT1Cluster.clusterNum % 100000 == 0) {
            std::cout << "\rProgress: " << ((float) btcT1Cluster.clusterNum / btcT1ClusteringCount) * 100 << "%" << std::flush;
        }

        for (auto btcT1Addr : btcT1ClusterDedupAddrs) {
            auto clusterInBtcT0 = btcClusteringT0.getCluster(blocksci::RawAddress{btcT1Addr.scriptNum, reprType(btcT1Addr.type)});
            if (clustersInBtcT0.find(clusterInBtcT0->clusterNum) == clustersInBtcT0.end()) {
                clustersInBtcT0.insert(clusterInBtcT0->clusterNum);
            }
        }
        if (clustersInBtcT0.size() > 1) {
            mergesBtcT1vsT0 += clustersInBtcT0.size();
        }
    }

    // mergesBtcT1vsT0=71670998
    std::cout << std::endl << std::endl << "mergesBtcT1vsT0=" << mergesBtcT1vsT0 << std::endl;

    RANGES_FOR (auto cccCluster, cccClusteringT0.getClusters()) {
        uint32_t cccClusterSize = cccCluster.getTypeEquivSize();

        if (cccCluster.clusterNum % 100000 == 0) {
            std::cout << "\rProgress: " << ((float) cccCluster.clusterNum / cccClusteringCount) * 100 << "%" << std::flush;
        }

        if (cccClusterSize == 1 || cccClusterSize > 1000) {
            continue;
        }

        auto cccClusterDedupAddrs = cccCluster.getDedupAddresses();
        std::unordered_set<uint32_t> clustersInBtcT0;
        std::unordered_set<uint32_t> clustersInBtcT1;

        for (auto cccDedupAddr : cccClusterDedupAddrs) {
            auto clusterInBtcT0 = btcClusteringT0.getCluster(blocksci::RawAddress{cccDedupAddr.scriptNum, reprType(cccDedupAddr.type)});
            if (clustersInBtcT0.find(clusterInBtcT0->clusterNum) == clustersInBtcT0.end()) {
                clustersInBtcT0.insert(clusterInBtcT0->clusterNum);
            }

            auto clusterInBtcT1 = btcClusteringT1.getCluster(blocksci::RawAddress{cccDedupAddr.scriptNum, reprType(cccDedupAddr.type)});
            if (clustersInBtcT1.find(clusterInBtcT1->clusterNum) == clustersInBtcT1.end()) {
                clustersInBtcT1.insert(clusterInBtcT1->clusterNum);
            }
        }

        uint32_t clustersInBtcT0Count, clustersInBtcT1Count;
        clustersInBtcT0Count = clustersInBtcT0.size();
        clustersInBtcT1Count = clustersInBtcT1.size();
        if (clustersInBtcT1Count < clustersInBtcT0Count) {
            fout_results
                << cccCluster.clusterNum << ","
                << cccClusterSize << ","
                << clustersInBtcT0Count << ","
                << clustersInBtcT1Count
                << std::endl;
        }
    }

    return 0;
}