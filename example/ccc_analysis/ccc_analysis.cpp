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

    auto lastBlockBeforeFork = btc[478558];
    uint32_t lastTxIndexBeforeFork = lastBlockBeforeFork.endTxIndex() - 1;

    auto mainClustering = blocksci::ClusterManager("/mnt/data/blocksci/ccc_python_btc_bch_reduceto_btc__", btc.getAccess());
    auto referenceClustering = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc__", btc.getAccess());
    auto forkHeightClustering = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc_478558__", btc.getAccess());

    std::cout << "mainClustering.getClusterCount()=" << mainClustering.getClusterCount() << std::endl;

    std::fstream fout_ccc_clusters, fout_ccc_clusters_relevant, fout_cluster_components, fout_cluster_components_relevant, fout_cluster_tags;
    fout_ccc_clusters.open("/mnt/data/analysis/ccc_clusters_all.csv", std::ios::out | std::ios::app);
    fout_ccc_clusters_relevant.open("/mnt/data/analysis/ccc_clusters_relevant.csv", std::ios::out | std::ios::app);
    fout_cluster_components.open("/mnt/data/analysis/ccc_cluster_components_all.csv", std::ios::out | std::ios::app);
    fout_cluster_components_relevant.open("/mnt/data/analysis/ccc_cluster_components_relevant.csv", std::ios::out | std::ios::app);
    fout_cluster_tags.open("/mnt/data/analysis/ccc_cluster_tags.csv", std::ios::out | std::ios::app);

    fout_ccc_clusters
        << "clusterId,"
        << "clusterSize,"
        << "bchAddresses,"
        << "btcOnlyCluster,"
        << "sameAsOnForkHeight,"
        << "clustersInReference,"
        << "smallestReferenceClusterSize,"
        << "largestReferenceClusterSize,"
        << "additionallyMergedAddresses,"
        << "clusterGrowth,"
        << "tags,"
        << "tagCount"
        << "\n";

    fout_ccc_clusters_relevant
        << "clusterId,"
        << "clusterSize,"
        << "bchAddresses,"
        << "btcOnlyCluster,"
        << "sameAsOnForkHeight,"
        << "clustersInReference,"
        << "smallestReferenceClusterSize,"
        << "largestReferenceClusterSize,"
        << "additionallyMergedAddresses,"
        << "clusterGrowth,"
        << "tags,"
        << "tagCount"
        << "\n";

    fout_cluster_components
        << "clusterId,"
        << "referenceClusterId,"
        << "referenceClusterSize"
        << "\n";

    fout_cluster_components_relevant
        << "clusterId,"
        << "referenceClusterId,"
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

    std::unordered_map<blocksci::DedupAddress, std::string> tags = getTags(btc);

    RANGES_FOR (auto cluster, mainClustering.getClusters()) {
        uint32_t clusterSize = cluster.getTypeEquivSize();
        auto clusterDedupAddresses = cluster.getDedupAddresses();

        uint32_t smallestReferenceClusterSize = std::numeric_limits<uint32_t>::max();
        uint32_t largestReferenceClusterSize = 0;

        std::unordered_map<uint32_t, uint32_t> clustersInReferenceData;
        std::unordered_set<std::string> clusterTags;

        for (auto dedupAddress : clusterDedupAddresses) {
            auto bchAddressScriptBase = blocksci::Address{dedupAddress.scriptNum, reprType(dedupAddress.type), bch.getAccess()}.getBaseScript();
            if (bchAddressScriptBase.hasBeenSeen()) {
                ++bchAddressCount;
            }

            if (tags.find(dedupAddress) != tags.end()) {
                std::pair<std::unordered_set<std::string>::iterator, bool> insertResult = clusterTags.insert(tags[dedupAddress]);
                if (insertResult.second) {
                    fout_cluster_tags
                        << cluster.clusterNum << ","
                        << tags[dedupAddress]
                        << "\n";
                }
            }

            auto clusterInReferenceData = referenceClustering.getCluster(blocksci::RawAddress{dedupAddress.scriptNum, reprType(dedupAddress.type)});
            if (clusterInReferenceData) {
                uint32_t referenceClusterSize = clusterInReferenceData->getTypeEquivSize();
                clustersInReferenceData.insert({clusterInReferenceData->clusterNum, referenceClusterSize});
            }
            else {
                addressesMissingInReferenceClustering++;
            }
        }

        uint32_t clusterCountInReferenceData = clustersInReferenceData.size();

        if (bchAddressCount == 0) {
            btcOnlyCluster = true;
            //sameAsOnForkHeight = false;
            btcOnlyClusterCount++;
        }
        else {
            btcOnlyCluster = false;

            // check if the cluster has (not) changed since the BCH fork (block height 478588)
            auto firstDedupAddress = *(cluster.getDedupAddresses().begin());
            auto firstClusterInReferenceData = forkHeightClustering.getCluster(blocksci::RawAddress{firstDedupAddress.scriptNum, reprType(firstDedupAddress.type)});
            if (firstClusterInReferenceData) {
                if (clusterSize == 1) {
                    auto btcAddressScriptBase = blocksci::Address{firstDedupAddress.scriptNum, reprType(firstDedupAddress.type), btc.getAccess()}.getBaseScript();
                    // check if address existed before fork, if yes -> true, false otherwise
                    if (*btcAddressScriptBase.getFirstTxIndex() <= lastTxIndexBeforeFork) {
                        sameAsOnForkHeight = true;
                        ++sameAsOnForkHeightCount;
                    }
                    else {
                        sameAsOnForkHeight = false;
                    }
                }
                else if (clusterSize == firstClusterInReferenceData->getTypeEquivSize()) {
                    sameAsOnForkHeight = true;
                    ++sameAsOnForkHeightCount;
                }
            }
            else {
                sameAsOnForkHeight = false;
            }
        }

        for (const auto& cl : clustersInReferenceData) {
            if (cl.second < smallestReferenceClusterSize) {
                smallestReferenceClusterSize = cl.second;
            }
            if (cl.second > largestReferenceClusterSize) {
                largestReferenceClusterSize = cl.second;
            }

            fout_cluster_components
                << cluster.clusterNum << ","
                << cl.first << "," // reference cluster ID
                << cl.second       // reference cluster size
                << "\n";

            // only if ccc-cluster contains several single-chain clusters
            if (clusterCountInReferenceData > 1) {
                fout_cluster_components_relevant
                    << cluster.clusterNum << ","
                    << cl.first << "," // reference cluster ID
                    << cl.second       // reference cluster size
                    << "\n";
            }
        }

        fout_ccc_clusters
            << cluster.clusterNum << ","
            << clusterSize << ","
            << bchAddressCount << ","
            << btcOnlyCluster << ","
            << sameAsOnForkHeight << ","
            << clusterCountInReferenceData << ","
            << smallestReferenceClusterSize << ","
            << largestReferenceClusterSize << ","
            << (clusterSize - largestReferenceClusterSize) << ","
            << ((float) clusterSize / largestReferenceClusterSize - 1) << ","
            // "implode" clusterTag set
            << std::accumulate(clusterTags.begin(), clusterTags.end(), std::string(),
            [](const std::string& a, const std::string& b) -> std::string {
               return a + (a.length() > 0 ? ";" : "") + b;
            } ) << ","
            << clusterTags.size()
            << "\n";

        // only if ccc-cluster contains several single-chain clusters
        if (clusterCountInReferenceData > 1) {
            fout_ccc_clusters_relevant
                << cluster.clusterNum << ","
                << clusterSize << ","
                << bchAddressCount << ","
                << btcOnlyCluster << ","
                << sameAsOnForkHeight << ","
                << clusterCountInReferenceData << ","
                << smallestReferenceClusterSize << ","
                << largestReferenceClusterSize << ","
                << (clusterSize - largestReferenceClusterSize) << ","
                << ((float) clusterSize / largestReferenceClusterSize - 1) << ","
                // "implode" clusterTag set
                << std::accumulate(clusterTags.begin(), clusterTags.end(), std::string(),
                                   [](const std::string& a, const std::string& b) -> std::string {
                                       return a + (a.length() > 0 ? ";" : "") + b;
                                   } ) << ","
                << clusterTags.size()
                << "\n";
        }

        bchAddressCount = 0;
        ++clusterIndex;
    }

    fout_ccc_clusters.close();
    fout_ccc_clusters_relevant.close();
    fout_cluster_components.close();
    fout_cluster_components_relevant.close();
    fout_cluster_tags.close();

    std::cout << "clusterIndex=" << clusterIndex << std::endl;
    std::cout << "btcOnlyClusterCount=" << btcOnlyClusterCount << std::endl;
    std::cout << "sameAsOnForkHeightCount=" << sameAsOnForkHeightCount << std::endl;
    std::cout << "addressesMissingInReferenceClustering=" << addressesMissingInReferenceClustering << std::endl;

    return 0;
}
