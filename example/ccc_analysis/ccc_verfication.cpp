#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>
#include <internal/address_info.hpp>

#include <iostream>
#include <string>


int main(int argc, char * argv[]) {
    blocksci::Blockchain btc{"/mnt/data/blocksci/bitcoin/595303-root-v0.6-0e6e863/config.json"};

    auto cccClusteringT0 = blocksci::ClusterManager("/mnt/data/blocksci/ccc_python_btc_bch_reduceto_btc__", btc.getAccess());
    auto btcClusteringT0 = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc__", btc.getAccess());

    std::vector<std::pair<blocksci::DedupAddress, blocksci::DedupAddress>> cccEdges;

    uint64_t edgeCount = 0;

    uint32_t cccClusterCount = cccClusteringT0.getClusterCount();

    RANGES_FOR (auto cccCluster, cccClusteringT0.getClusters()) {
        uint32_t cccClusterSize = cccCluster.getTypeEquivSize();

        if (cccCluster.clusterNum  % 100000 == 0) {
            std::cout << "\r" << (float) cccCluster.clusterNum / cccClusterCount * 100 << "% done, " << cccEdges.size() << "edges" << std::flush;
        }

        // > 50: 5313677
        // > 100: ~8mio. edges
        // > 1000: 36195754
        // > 10000: 388234073
        if (cccClusterSize == 1 || cccClusterSize > 50) {
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

    for (auto addrPair : cccEdges) {
        auto cccCluster1 = cccClusteringT0.getCluster({addrPair.first.scriptNum, blocksci::reprType(addrPair.first.type)});
        auto cccCluster2 = cccClusteringT0.getCluster({addrPair.second.scriptNum, blocksci::reprType(addrPair.second.type)});

        if (cccCluster1 && cccCluster2 && cccCluster1->clusterNum != cccCluster2->clusterNum) {
            std::cout << "error1" << std::endl;
        }

        auto btcCluster1 = btcClusteringT0.getCluster({addrPair.first.scriptNum, blocksci::reprType(addrPair.first.type)});
        auto btcCluster2 = btcClusteringT0.getCluster({addrPair.second.scriptNum, blocksci::reprType(addrPair.second.type)});

        if (btcCluster1 && btcCluster2 && btcCluster1->clusterNum == btcCluster2->clusterNum) {
            std::cout << "error2" << std::endl;
        }
    }
}
