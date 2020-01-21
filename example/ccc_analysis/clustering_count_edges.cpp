#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>

#include <iostream>
#include <string>


int main(int argc, char * argv[]) {
    blocksci::Blockchain btc{"/mnt/data/blocksci/bitcoin/595303-root-v0.6-0e6e863/config.json"};

    auto clustering = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc__", btc.getAccess());

    uint64_t edgeCount = 0;

    RANGES_FOR (auto cluster, clustering.getClusters()) {
        uint32_t clusterSize = cluster.getTypeEquivSize();

        if (clusterSize == 1 || clusterSize > 100) {
            continue;
        }

        edgeCount += clusterSize * (clusterSize - 1) / 2;
    }

    std::cout << "edgeCount = " << edgeCount << std::endl;
    return 0;
}