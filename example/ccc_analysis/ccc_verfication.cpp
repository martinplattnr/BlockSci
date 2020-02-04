#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>
#include <internal/address_info.hpp>

#include <iostream>
#include <string>


int main(int argc, char * argv[]) {
    blocksci::Blockchain btc{"/mnt/data/blocksci/bitcoin/595303-root-v0.6-0e6e863/config.json"};

    auto cccClusteringT0 = blocksci::ClusterManager("/mnt/data/blocksci/ccc_python_btc_bch_reduceto_btc_with_log__534602", btc.getAccess());
    auto btcClusteringT0 = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc_534602", btc.getAccess());

    auto btcClusteringT1 = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc_587985", btc.getAccess());

    std::vector<std::pair<blocksci::DedupAddress, blocksci::DedupAddress>> cccEdges;
    uint64_t cccEdgeCount = 0;

    uint32_t cccClusterCount = cccClusteringT0.getClusterCount();

    uint32_t excludedCCCClusters = 0;
    uint64_t foundEdgesInBtcT1 = 0;

    RANGES_FOR (auto cccCluster, cccClusteringT0.getClusters()) {
        uint32_t cccClusterSize = cccCluster.getTypeEquivSize();

        if (cccCluster.clusterNum && cccCluster.clusterNum  % 100000 == 0) {
            std::cout << "\r" << (float) cccCluster.clusterNum / cccClusterCount * 100 << "% done, " << cccEdgeCount << "edges, "
                << foundEdgesInBtcT1 << " found in BTCT1 (" << (float) foundEdgesInBtcT1 / cccEdgeCount * 100 << "%)" << std::flush;
        }

        if (cccClusterSize == 1) {
            // exclude clusters with only one address
            continue;
        }
        // > 50: 5313677
        // > 100: ~8mio. edges

        // > 1000: 36195754
        /* /root/clion_remote/cmake-build-debug-remote/example/ccc_analysis/ccc_verfication
            99.9899% done, 27601773edges
            additional edges via ccc: 27601773
            excluded ccc clusters: 6068
            Found 1377506 of 27601773 edges in BTC at t=1 (0.0499064 %)

            Process finished with exit code 0
            */

        // > 10000: 388234073

        // malte: sample randomly, not by size

        /*
        if (cccClusterSize > 10000) {
            ++excludedCCCClusters;
            continue;
        }
        */


        std::unordered_set<uint32_t> processedScClusters;
        std::vector<blocksci::DedupAddress> addrsToLink;

        bool firstIteration = true;
        auto cccClusterDedupAddrs = cccCluster.getDedupAddresses();

        // find all corresponding single-chain clusters
        for (auto cccDedupAddress : cccClusterDedupAddrs) {
            auto clusterInBtcT0 = btcClusteringT0.getCluster(blocksci::RawAddress{cccDedupAddress.scriptNum, reprType(cccDedupAddress.type)});
            if (processedScClusters.find(clusterInBtcT0->clusterNum) == processedScClusters.end()) {
                addrsToLink.push_back(clusterInBtcT0->getDedupAddresses()[0]); // it might be sufficient to only have one set for addresses, and none for processedScClusters
                processedScClusters.insert(clusterInBtcT0->clusterNum);
            }
        }

        /* > 10000
         * 99.9899% done, 216534756edges, 58589026 found in BTCT1 (27.0576%)
            additional *relevant* edges via ccc: 0
            excluded ccc clusters: 2
            Found 58589026 of 0 edges in BTC at t=1 (inf %)
         */

        if (addrsToLink.size() > 10000) {
            std::cout << addrsToLink.size() << std::endl;
            //excludedCCCClusters++;
            //std::cout << "cluster " << cccCluster.clusterNum << std::endl;
            //std::cout << "  processedScClusters=" << processedScClusters.size() << std::endl;
            //std::cout << "  addrsToLink=" << addrsToLink.size() << std::endl << std::endl;
            //continue;
        }
        continue;

        for(uint32_t i = 0; i != addrsToLink.size(); i++) {
            for(uint32_t j = i + 1; j != addrsToLink.size(); j++) {
                cccEdgeCount++;
                auto cccCluster1 = btcClusteringT1.getCluster({addrsToLink[i].scriptNum, blocksci::reprType(addrsToLink[i].type)});
                auto cccCluster2 = btcClusteringT1.getCluster({addrsToLink[j].scriptNum, blocksci::reprType(addrsToLink[j].type)});
                if (cccCluster1->clusterNum == cccCluster2->clusterNum) {
                    foundEdgesInBtcT1++;
                }
                //cccEdges.push_back({addrsToLink[i], addrsToLink[j]});
            }
        }
    }
    std::cout << std::endl;

    std::cout << "additional *relevant* edges via ccc: " << cccEdges.size() << std::endl;
    std::cout << "excluded ccc clusters: " << excludedCCCClusters << std::endl;


    /*
    for (auto addrPair : cccEdges) {
        auto cccCluster1 = btcClusteringT1.getCluster({addrPair.first.scriptNum, blocksci::reprType(addrPair.first.type)});
        auto cccCluster2 = btcClusteringT1.getCluster({addrPair.second.scriptNum, blocksci::reprType(addrPair.second.type)});

        if (cccCluster1->clusterNum == cccCluster2->clusterNum) {
            foundEdgesInBtcT1++;
        }
    }
    */

    std::cout << "Found " << foundEdgesInBtcT1 << " of " << cccEdges.size() << " edges in BTC at t=1 (" << (double) foundEdgesInBtcT1 / cccEdges.size() *100 << " %)" << std::endl;

    return 0;
}
