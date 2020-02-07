//
// Created by martin on 15.11.19.
//


#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>
#include <internal/address_info.hpp>

#include <iostream>
#include <string>

std::unordered_map<blocksci::DedupAddress, std::pair<std::string, std::string>> getTags(blocksci::Blockchain& chain);

std::string implode(std::unordered_set<std::string> iterable) {
    return std::accumulate(iterable.begin(), iterable.end(), std::string(),
            [](const std::string& a, const std::string& b) -> std::string {
                return a + (a.length() > 0 ? "," : "") + b;
            } );
}

int main(int argc, char * argv[]) {
    blocksci::Blockchain btc {"/mnt/data/blocksci/btc-bch/2017-12-31/btc/config.json"};
    blocksci::Blockchain bch {"/mnt/data/blocksci/btc-bch/2017-12-31/bch/config.json"};

    auto &scripts = btc.getAccess().scripts;

    std::unordered_map<blocksci::DedupAddress, uint32_t> addressLastUsageBtc;
    addressLastUsageBtc.reserve(scripts->totalAddressCount());

    // get and store block height of last usage for every address
    RANGES_FOR(auto block, btc) {
        RANGES_FOR(auto tx, block) {
            RANGES_FOR(auto output, tx.outputs()) {
                addressLastUsageBtc[{output.getAddress().scriptNum, blocksci::dedupType(output.getAddress().type)}] = block.height();
                if (blocksci::dedupType(output.getAddress().type) == blocksci::DedupAddressType::SCRIPTHASH) {
                    auto scriptHashData = blocksci::script::ScriptHash(output.getAddress().scriptNum, btc.getAccess());
                    auto wrappedAddr = scriptHashData.getWrappedAddress();
                    if (wrappedAddr) {
                        addressLastUsageBtc[{wrappedAddr->scriptNum, blocksci::dedupType(wrappedAddr->type)}] =  block.height();
                    }
                }
            }
            RANGES_FOR(auto input, tx.inputs()) {
                addressLastUsageBtc[{input.getAddress().scriptNum, blocksci::dedupType(input.getAddress().type)}] = block.height();
                if (blocksci::dedupType(input.getAddress().type) == blocksci::DedupAddressType::SCRIPTHASH) {
                    auto scriptHashData = blocksci::script::ScriptHash(input.getAddress().scriptNum, btc.getAccess());
                    auto wrappedAddr = scriptHashData.getWrappedAddress();
                    if (wrappedAddr) {
                        addressLastUsageBtc[{wrappedAddr->scriptNum, blocksci::dedupType(wrappedAddr->type)}] =  block.height();
                    }
                }
            }
        }
    }

    auto lastBlockBeforeFork = btc[478558];
    uint32_t lastTxIndexBeforeFork = lastBlockBeforeFork.endTxIndex() - 1;

    // load clusterings
    auto ccClustering = blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc-bch/btc/2017-12-31", btc.getAccess());
    auto scClustering = blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc/2017-12-31", btc.getAccess());
    auto forkHeightClustering = blocksci::ClusterManager("/mnt/data/blocksci/c_python_btc_478558__", btc.getAccess());

    uint32_t ccClusteringCount = ccClustering.getClusterCount();

    std::cout << "ccClustering.getClusterCount()=" << ccClusteringCount << std::endl << std::endl;

    // open output files
    std::fstream fout_ccc_clusters, fout_ccc_clusters_relevant, fout_cluster_components, fout_cluster_components_relevant, fout_cluster_tags;
    fout_ccc_clusters.open("/mnt/data/analysis/ccc_analysis/ccc_clusters_all.csv", std::ios::out | std::ios::app);
    fout_ccc_clusters_relevant.open("/mnt/data/analysis/ccc_analysis/ccc_clusters_relevant.csv", std::ios::out | std::ios::app);
    fout_cluster_components.open("/mnt/data/analysis/ccc_analysis/ccc_cluster_components_all.csv", std::ios::out | std::ios::app);
    fout_cluster_components_relevant.open("/mnt/data/analysis/ccc_analysis/ccc_cluster_components_relevant.csv", std::ios::out | std::ios::app);
    fout_cluster_tags.open("/mnt/data/analysis/ccc_analysis/ccc_cluster_tags.csv", std::ios::out | std::ios::app);

    std::stringstream ccc_clusters_headers, cluster_components_headers;

    // write CSV file headers
    ccc_clusters_headers
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
        //<< "balance"
        << "lastUsage,"
        << "tagCategories,"
        << "tagEntities,"
        << "tagCount"
        << "\n";

    fout_ccc_clusters << ccc_clusters_headers.str();
    fout_ccc_clusters_relevant << ccc_clusters_headers.str();

    cluster_components_headers
        << "clusterId,"
        << "referenceClusterId,"
        << "referenceClusterSize,"
        << "lastUsage,"
        << "tagCategories,"
        << "tagEntities,"
        << "tagCount"
        << "\n";

    fout_cluster_components << cluster_components_headers.str();
    fout_cluster_components_relevant << cluster_components_headers.str();

    fout_cluster_tags
        << "clusterId,"
        << "category,"
        << "entity"
        << "\n";

    uint32_t addressesMissingInScClustering = 0;
    uint32_t clusterIndex = 0;
    uint32_t bchAddressCount = 0;

    bool btcOnlyCluster = false;
    uint32_t btcOnlyClusterCount = 0;

    bool sameAsOnForkHeight = false;
    uint32_t sameAsOnForkHeightCount = 0;

    // generate tag map
    std::unordered_map<blocksci::DedupAddress, std::pair<std::string, std::string>> tags = getTags(btc);

    using ReferenceClusterInfo = std::unordered_map<uint32_t, std::tuple<uint32_t, uint32_t, std::unordered_set<std::string>, std::unordered_set<std::string>>>;

    RANGES_FOR (auto cluster, ccClustering.getClusters()) {
        uint32_t clusterSize = cluster.getTypeEquivSize();
        auto clusterDedupAddresses = cluster.getDedupAddresses();

        uint32_t smallestReferenceClusterSize = std::numeric_limits<uint32_t>::max();
        uint32_t largestReferenceClusterSize = 0;

        //uint64_t clusterBalance = cluster.calculateBalance(-1);
        uint32_t clusterLastUsageBtc = 0;

        ReferenceClusterInfo clustersInReferenceData;
        clustersInReferenceData.reserve(clusterSize);

        std::unordered_set<std::string> clusterTagCategories;
        std::unordered_set<std::string> clusterTags;

        if (cluster.clusterNum % 100000 == 0) {
            std::cout << "\rProgress: " << ((float) cluster.clusterNum / ccClusteringCount) << "%" << std::flush;
        }

        for (auto dedupAddress : clusterDedupAddresses) {
            auto bchAddressScriptBase = blocksci::Address{dedupAddress.scriptNum, reprType(dedupAddress.type), bch.getAccess()}.getBaseScript();
            if (bchAddressScriptBase.hasBeenSeen()) {
                ++bchAddressCount;
            }

            // determine last usage of current address and update last usage of cluster if needed
            if (addressLastUsageBtc[dedupAddress] > clusterLastUsageBtc) {
                clusterLastUsageBtc = addressLastUsageBtc[dedupAddress];
            }

            // check if a tag can be assigned to the current address
            if (tags.find(dedupAddress) != tags.end()) {
                clusterTagCategories.insert(tags[dedupAddress].first);
                std::pair<std::unordered_set<std::string>::iterator, bool> insertResult = clusterTags.insert(tags[dedupAddress].second);
                if (insertResult.second) {
                    fout_cluster_tags
                        << cluster.clusterNum << ","
                        << tags[dedupAddress].first << ","
                        << tags[dedupAddress].second
                        << "\n";
                }
            }

            // find the cluster in the reference clustering that contains the current address
            auto clusterInReferenceData = scClustering.getCluster(blocksci::RawAddress{dedupAddress.scriptNum, reprType(dedupAddress.type)});
            if (clusterInReferenceData) {
                uint32_t referenceClusterSize = clusterInReferenceData->getTypeEquivSize();

                // check if reference cluster is the smallest/largest seen so far
                if (referenceClusterSize < smallestReferenceClusterSize) {
                    smallestReferenceClusterSize = referenceClusterSize;
                }
                if (referenceClusterSize > largestReferenceClusterSize) {
                    largestReferenceClusterSize = referenceClusterSize;
                }

                if (clustersInReferenceData.find(clusterInReferenceData->clusterNum) == clustersInReferenceData.end()) {
                    std::unordered_set<std::string> referenceClusterTagCategories;
                    std::unordered_set<std::string> referenceClusterTags;
                    uint32_t referenceClusterLastUsageBtc = 0;

                    // determine tags of reference cluster
                    auto referenceClusterDedupAddresses = clusterInReferenceData->getDedupAddresses();
                    for (auto referenceDedupAddress : referenceClusterDedupAddresses) {
                        // check if a tag can be assigned to the current address
                        if (tags.find(referenceDedupAddress) != tags.end()) {
                            referenceClusterTagCategories.insert(tags[referenceDedupAddress].first);
                            referenceClusterTags.insert(tags[referenceDedupAddress].second);
                        }

                        // determine last usage of current address and update last usage of reference cluster if needed
                        if (addressLastUsageBtc[referenceDedupAddress] > referenceClusterLastUsageBtc) {
                            referenceClusterLastUsageBtc = addressLastUsageBtc[referenceDedupAddress];
                        }
                    }

                    clustersInReferenceData.insert({clusterInReferenceData->clusterNum, std::make_tuple(referenceClusterSize, referenceClusterLastUsageBtc, referenceClusterTagCategories, referenceClusterTags)});
                }
            }
            else {
                addressesMissingInScClustering++;
            }
        }

        uint32_t clusterCountInReferenceData = clustersInReferenceData.size();

        if (bchAddressCount == 0) {
            btcOnlyCluster = true;
            sameAsOnForkHeight = false;
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
                else {
                    if (clusterSize == firstClusterInReferenceData->getTypeEquivSize()) {
                        sameAsOnForkHeight = true;
                        ++sameAsOnForkHeightCount;
                    }
                    else {
                        sameAsOnForkHeight = false;
                    }
                }
            }
            else {
                sameAsOnForkHeight = false;
            }
        }

        for (const auto& cl : clustersInReferenceData) {
            std::stringstream ccc_cluster_components_ss;
            ccc_cluster_components_ss
                << cluster.clusterNum << ","
                << cl.first << ","                 // reference cluster ID
                << std::get<0>(cl.second) << ","   // reference cluster size
                << std::get<1>(cl.second) << ","   // reference cluster last usage
                << "\"" << implode(std::get<2>(cl.second)) << "\"," // "implode" reference cluster tag categories
                << "\"" << implode(std::get<3>(cl.second)) << "\"," // "implode" reference cluster tags
                << std::get<3>(cl.second).size()   // reference cluster tag count
                << "\n";

            fout_cluster_components << ccc_cluster_components_ss.str();

            // only output if ccc-cluster contains several single-chain clusters
            if (clusterCountInReferenceData > 1) {
                fout_cluster_components_relevant << ccc_cluster_components_ss.str();
            }
        }

        std::stringstream ccc_clusters_ss;

        ccc_clusters_ss
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
            //<< clusterBalance << ","
            << clusterLastUsageBtc << ","
            << "\"" << implode(clusterTagCategories) << "\","
            << "\"" << implode(clusterTags) << "\","
            << clusterTags.size()
            << "\n";

        fout_ccc_clusters << ccc_clusters_ss.str();

        // only if ccc-cluster contains several single-chain clusters
        if (clusterCountInReferenceData > 1) {
            fout_ccc_clusters_relevant << ccc_clusters_ss.str();
        }

        bchAddressCount = 0;
        ++clusterIndex;
    }

    fout_ccc_clusters.close();
    fout_ccc_clusters_relevant.close();
    fout_cluster_components.close();
    fout_cluster_components_relevant.close();
    fout_cluster_tags.close();

    std::cout << std::endl << std::endl;
    std::cout << "clusterIndex=" << clusterIndex << std::endl;
    std::cout << "btcOnlyClusterCount=" << btcOnlyClusterCount << std::endl;
    std::cout << "sameAsOnForkHeightCount=" << sameAsOnForkHeightCount << std::endl;
    std::cout << "addressesMissingInScClustering=" << addressesMissingInScClustering << std::endl;

    return 0;
}
