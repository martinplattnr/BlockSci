//
// Created by martin on 15.11.19.
//


#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>

#include <iostream>

std::unordered_map<blocksci::DedupAddress, std::string> getTags(blocksci::Blockchain& chain);

int main(int argc, char * argv[]) {
    blocksci::Blockchain btc {"/mnt/data/blocksci/bitcoin/595303-root-v0.6-0e6e863/config.json"};
    blocksci::Blockchain bch {"/mnt/data/blocksci/bitcoin-cash/595303-fork-v0.6-0e6e863/config.json"};

    auto mainClustering = blocksci::ClusterManager("/mnt/data/blocksci/ccc_python_btc_bch_reduceto_btc__", btc.getAccess());
    auto referenceClustering = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc__", btc.getAccess());

    auto forkHeightClustering = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc_478558__", btc.getAccess());

    std::cout << "mainClustering.getClusterCount()=" << mainClustering.getClusterCount() << std::endl;

    std::fstream fout_main, fout_cluster_details, fout_cluster_tags;
    fout_main.open("/mnt/data/ccc_analysis_extended_with_tags.csv", std::ios::out | std::ios::app);
    fout_cluster_details.open("/mnt/data/ccc_analysis_extended_with_tags_cluster_details.csv", std::ios::out | std::ios::app);
    fout_cluster_tags.open("/mnt/data/ccc_analysis_extended_with_tags_cluster_tags.csv", std::ios::out | std::ios::app);

    fout_main
        << "clusterId,"
        << "clusterSize,"
        << "bchAddresses,"
        << "btcOnlyCluster,"
        << "sameAsOnForkHeight,"
        << "clustersInReference,"
        << "smallestReferenceClusterSize,"
        << "ratioSmallestReferenceClusterSizeToClusterSize,"
        << "ratioSmallestReferenceClusterSizeToAverageReferenceClusterSize,"
        << "clusterTags"
        << "\n";

    fout_cluster_details
        << "clusterId,"
        << "referenceClusterSize"
        << "\n";

    fout_cluster_tags
        << "clusterId,"
        << "tag"
        << "\n";

    uint32_t addressesMissingInReferenceClustering = 0;
    uint32_t clusterIndex = 0;
    uint32_t bchAddressCount = 0;

    bool btcOnlyCluster = false;
    uint32_t btcOnlyClusterCount = 0;

    bool sameAsOnForkHeight = false;
    uint32_t sameAsOnForkHeightCount = 0;

    std::unordered_set<uint32_t> clustersInReferenceData;

    std::unordered_map<blocksci::DedupAddress, std::string> tags = getTags(btc);

    RANGES_FOR (auto cluster, mainClustering.getClusters()) {
        uint32_t clusterSize = cluster.getTypeEquivSize();
        auto clusterDedupAddresses = cluster.getDedupAddresses();
        std::string clusterTags;

        uint32_t smallestClusterSize = std::numeric_limits<uint32_t>::max();
        for (auto dedupAddress : clusterDedupAddresses) {
            auto bchAddressScriptBase = blocksci::Address{dedupAddress.scriptNum, reprType(dedupAddress.type), bch.getAccess()}.getBaseScript();
            if (bchAddressScriptBase.hasBeenSeen()) {
                ++bchAddressCount;
            }

            if (tags.find(dedupAddress) != tags.end()) {
                fout_cluster_tags
                    << cluster.clusterNum << ","
                    << tags[dedupAddress]
                    << "\n";
                clusterTags += tags[dedupAddress] + ", ";
            }

            auto clusterInReferenceData = referenceClustering.getCluster(blocksci::RawAddress{dedupAddress.scriptNum, reprType(dedupAddress.type)});
            // todo: if address is not seen before fork block height, don't try to retrieve cluster.
            if (clusterInReferenceData) {
                std::pair<std::unordered_set<uint32_t>::iterator, bool> insertResult = clustersInReferenceData.insert(clusterInReferenceData->clusterNum);
                if (insertResult.second) {
                    // clusterNum was inserted (for the first time)
                    uint32_t referenceClusterSize = clusterInReferenceData->getTypeEquivSize();
                    if (referenceClusterSize < smallestClusterSize) {
                        smallestClusterSize = referenceClusterSize;
                    }

                    fout_cluster_details
                        << cluster.clusterNum << ","
                        << referenceClusterSize
                        << "\n";
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

        uint32_t clusterCountInReferenceData = clustersInReferenceData.size();

        if (bchAddressCount == 0) {
            btcOnlyCluster = true;
            btcOnlyClusterCount++;
        }
        else {
            btcOnlyCluster = false;
        }

        fout_main
            << cluster.clusterNum << ","
            << clusterSize << ","
            << bchAddressCount << ","
            << btcOnlyCluster << ","
            << sameAsOnForkHeight << ","
            << clusterCountInReferenceData << ","
            << smallestClusterSize << ","
            << (float) smallestClusterSize / clusterSize << ","
            << (float) smallestClusterSize / ((float) clusterSize / clusterCountInReferenceData) << ","
            << clusterTags
            << "\n";

        clustersInReferenceData.clear();
        bchAddressCount = 0;
        ++clusterIndex;
    }

    fout_main.close();
    fout_cluster_details.close();
    fout_cluster_tags.close();

    std::cout << "clusterIndex=" << clusterIndex << std::endl;
    std::cout << "btcOnlyClusterCount=" << btcOnlyClusterCount << std::endl;
    std::cout << "sameAsOnForkHeightCount=" << sameAsOnForkHeightCount << std::endl;
    std::cout << "addressesMissingInReferenceClustering=" << addressesMissingInReferenceClustering << std::endl;

    return 0;
}
