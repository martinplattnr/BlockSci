//
// Created by martin on 05.02.20.
//

#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>

#include <iostream>
#include <string>


int main(int argc, char * argv[]) {
    blocksci::Blockchain btc {"/mnt/hdd/blocksci/btc-bch/595303/btc/config.json"};
    blocksci::Blockchain bch {"/mnt/hdd/blocksci/btc-bch/595303/bch/config.json"};

    auto ccClustering = blocksci::ClusterManager("/mnt/data/blocksci/clusterings/ccc_python_btc_bch_reduceto_btc__", btc.getAccess());
    auto scClustering = blocksci::ClusterManager("/mnt/data/blocksci/clusterings/c_python_btc2__", btc.getAccess());

    uint32_t ccClusterCount = ccClustering.getClusterCount();
    std::cout << "ccClustering.getClusterCount()=" << ccClusterCount << std::endl << std::endl;

    std::fstream fout_ccc_comparison_quick;

    uint32_t ccMerges = 0;
    uint32_t affectedAddresses = 0;
    RANGES_FOR (auto ccCluster, ccClustering.getClusters()) {
        uint32_t ccClusterSize = ccCluster.getTypeEquivSize();
        auto ccDedupAddrs = ccCluster.getDedupAddresses();

        if (ccCluster.clusterNum % 100000 == 0) {
            std::cout << "\rProgress: " << ((float) ccCluster.clusterNum / ccClusterCount) << "%" << std::flush;
        }

        std::unordered_set<uint32_t> clustersInSc;
        for (auto ccDedupAddr : ccDedupAddrs) {
            // find the cluster in the single-chain clustering that contains the current address
            auto scCluster = scClustering.getCluster(blocksci::RawAddress{ccDedupAddr.scriptNum, reprType(ccDedupAddr.type)});
            if (scCluster) {
                if (clustersInSc.find(scCluster->clusterNum) == clustersInSc.end()) {
                    clustersInSc.insert(scCluster->clusterNum);
                }
            }
        }

        if (clustersInSc.size() > 1) {
            affectedAddresses += ccCluster.getSize();
            ccMerges += clustersInSc.size() - 1;
        }
    }

    std::cout << ccMerges << " merges found" << std::endl;
    std::cout << affectedAddresses << " affected addresses" << std::endl;
}
