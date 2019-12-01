//
// Created by martin on 15.11.19.
//


#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>

#include <iostream>

int main(int argc, char * argv[]) {
    blocksci::Blockchain btc {"/mnt/data/blocksci/bitcoin/595303-root-v0.6-0e6e863/config.json"};
    blocksci::Blockchain bch {"/mnt/data/blocksci/bitcoin-cash/595303-fork-v0.6-0e6e863/config.json"};

    auto mainClustering = blocksci::ClusterManager("/mnt/data/blocksci/ccc_python_btc_bch_reduceto_btc__", btc.getAccess());
    auto referenceClustering = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc__", btc.getAccess());

    auto forkHeightClustering = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc_478558__", btc.getAccess());

    std::vector<uint32_t> clusterCountInReferenceData(mainClustering.getClusterCount());
    std::cout << "mainClustering.getClusterCount()=" << mainClustering.getClusterCount() << std::endl;

    std::fstream fout;
    fout.open("/mnt/data/ccc_analysis_extended.csv", std::ios::out | std::ios::app);

    fout << "clusterId,"
         << "clusterSize,"
         << "bchAddresses,"
         << "btcOnlyCluster,"
         << "sameAsOnForkHeight,"
         << "clustersInReference,"
         << "smallestReferenceClusterSize,"
         << "ratioSmallestReferenceClusterSizeToClusterSize,"
         << "ratioSmallestReferenceClusterSizeToAverageReferenceClusterSize"
         << "\n";

    uint32_t addressesMissingInReferenceClustering = 0;
    uint32_t clusterIndex = 0;
    uint32_t bchAddressCount = 0;

    bool btcOnlyCluster = false;
    uint32_t btcOnlyClusterCount = 0;

    bool sameAsOnForkHeight = false;
    uint32_t sameAsOnForkHeightCount = 0;

    std::unordered_set<uint32_t> clustersInReferenceData;

    RANGES_FOR (auto cluster, mainClustering.getClusters()) {
        uint32_t clusterSize = cluster.getTypeEquivSize();
        auto clusterDedupAddresses = cluster.getDedupAddresses();

        uint32_t smallestClusterSize = std::numeric_limits<uint32_t>::max();
        for (auto dedupAddress : clusterDedupAddresses) {
            auto bchAddressScriptBase = blocksci::Address{dedupAddress.scriptNum, reprType(dedupAddress.type), bch.getAccess()}.getBaseScript();
            if (bchAddressScriptBase.hasBeenSeen()) {
                ++bchAddressCount;
            }

            auto clusterInReferenceData = referenceClustering.getCluster(blocksci::RawAddress{dedupAddress.scriptNum, reprType(dedupAddress.type)});
            // todo: if address is not seen before fork block height, don't try to retrieve cluster.
            if (clusterInReferenceData) {
                std::pair<std::unordered_set<uint32_t>::iterator, bool> insertResult = clustersInReferenceData.insert(clusterInReferenceData->clusterNum);
                if (insertResult.second) {
                    // clusterNum was inserted (for the first time)ch
                    uint32_t referenceClusterSize = clusterInReferenceData->getTypeEquivSize();
                    if (referenceClusterSize < smallestClusterSize) {
                        smallestClusterSize = referenceClusterSize;
                    }
                }
            }
            else {
                addressesMissingInReferenceClustering++;
            }
        }

        // check if the cluster has (not) changed since the BCH fork (block height 478588)
        auto firstDedupAddress = *(cluster.getDedupAddresses().begin());
        auto firstClusterInReferenceData = forkHeightClustering.getCluster(blocksci::RawAddress{firstDedupAddress.scriptNum, reprType(firstDedupAddress.type)});
        if (firstClusterInReferenceData && clusterSize == firstClusterInReferenceData->getTypeEquivSize()) {
            sameAsOnForkHeight = true;
            ++sameAsOnForkHeightCount;
        }
        else {
            sameAsOnForkHeight = false;
        }

        clusterCountInReferenceData[clusterIndex] = clustersInReferenceData.size();
        clustersInReferenceData.clear();

        if (bchAddressCount == 0) {
            btcOnlyCluster = true;
            btcOnlyClusterCount++;
        }
        else {
            btcOnlyCluster = false;
        }

        fout << cluster.clusterNum << ","
             << clusterSize << ","
             << bchAddressCount << ","
             << btcOnlyCluster << ","
             << sameAsOnForkHeight << ","
             << clusterCountInReferenceData[clusterIndex] << ","
             << smallestClusterSize << ","
             << (float) smallestClusterSize / clusterSize << ","
             << (float) smallestClusterSize / ((float) clusterSize / clusterCountInReferenceData[clusterIndex])
             << "\n";

        bchAddressCount = 0;
        ++clusterIndex;
    }
    fout.close();

    std::cout << "clusterIndex=" << clusterIndex << std::endl;
    std::cout << "btcOnlyClusterCount=" << btcOnlyClusterCount << std::endl;
    std::cout << "sameAsOnForkHeightCount=" << sameAsOnForkHeightCount << std::endl;
    std::cout << "addressesMissingInReferenceClustering=" << addressesMissingInReferenceClustering << std::endl;

    return 0;
}
