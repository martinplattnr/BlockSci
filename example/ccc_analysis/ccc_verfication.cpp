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

    uint32_t cccClusterCount = cccClusteringT0.getClusterCount();

    uint32_t excludedCCCClusters = 0;

    RANGES_FOR (auto cccCluster, cccClusteringT0.getClusters()) {
        uint32_t cccClusterSize = cccCluster.getTypeEquivSize();

        if (cccCluster.clusterNum  % 100000 == 0) {
            std::cout << "\r" << (float) cccCluster.clusterNum / cccClusterCount * 100 << "% done, " << cccEdges.size() << "edges" << std::flush;
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
        if (cccClusterSize > 10000) {
            ++excludedCCCClusters;
            continue;
        }

        std::unordered_set<blocksci::DedupAddress> processedAddresses;
        processedAddresses.reserve(cccClusterSize);

        bool firstIteration = true;
        auto cccClusterDedupAddrs = cccCluster.getDedupAddresses();
        for (auto cccDedupAddress : cccClusterDedupAddrs) {
            if (processedAddresses.find(cccDedupAddress) != processedAddresses.end()) {
                continue;
            }

            auto clusterInBtcT0 = btcClusteringT0.getCluster(blocksci::RawAddress{cccDedupAddress.scriptNum, reprType(cccDedupAddress.type)});
            auto clusterInBtcT0DedupAddrs = clusterInBtcT0->getDedupAddresses();
            if (firstIteration == false) {
                for (auto existingAddr : processedAddresses) {
                    for (auto btcT0DedupAddr : clusterInBtcT0DedupAddrs) {
                        cccEdges.push_back({existingAddr, btcT0DedupAddr});
                    }
                }
            }

            clusterInBtcT0DedupAddrs = clusterInBtcT0->getDedupAddresses();
            for (auto btcT0DedupAddr : clusterInBtcT0DedupAddrs) {
                processedAddresses.insert(btcT0DedupAddr);
            }

            firstIteration = false;
        }
    }
    std::cout << std::endl;

    std::cout << "additional edges via ccc: " << cccEdges.size() << std::endl;
    std::cout << "excluded ccc clusters: " << excludedCCCClusters << std::endl;

    uint64_t foundEdgesInBtcT1 = 0;

    for (auto addrPair : cccEdges) {
        auto cccCluster1 = btcClusteringT1.getCluster({addrPair.first.scriptNum, blocksci::reprType(addrPair.first.type)});
        auto cccCluster2 = btcClusteringT1.getCluster({addrPair.second.scriptNum, blocksci::reprType(addrPair.second.type)});

        if (cccCluster1->clusterNum == cccCluster2->clusterNum) {
            foundEdgesInBtcT1++;
        }
    }

    std::cout << "Found " << foundEdgesInBtcT1 << " of " << cccEdges.size() << " edges in BTC at t=1 (" << (double) foundEdgesInBtcT1 / cccEdges.size() << " %)" << std::endl;

    return 0;
}
