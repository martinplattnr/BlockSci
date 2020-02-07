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
        << "clustersInSc,"
        << "smallestScClusterSize,"
        << "largestScClusterSize,"
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
        << "scClusterId,"
        << "scClusterSize,"
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

    using ScClusterInfo = std::unordered_map<uint32_t, std::tuple<uint32_t, uint32_t, std::unordered_set<std::string>, std::unordered_set<std::string>>>;

    RANGES_FOR (auto ccCluster, ccClustering.getClusters()) {
        uint32_t ccClusterSize = ccCluster.getTypeEquivSize();
        auto ccClusterDedupAddrs = ccCluster.getDedupAddresses();

        uint32_t smallestScClusterSize = std::numeric_limits<uint32_t>::max();
        uint32_t largestScClusterSize = 0;

        //uint64_t ccClusterBalance = ccCluster.calculateBalance(-1);
        uint32_t ccClusterLastUsageBtc = 0;

        ScClusterInfo clustersInScData;
        clustersInScData.reserve(ccClusterSize);

        std::unordered_set<std::string> ccClusterTagCategories;
        std::unordered_set<std::string> ccClusterTags;

        if (ccCluster.clusterNum % 100000 == 0) {
            std::cout << "\rProgress: " << ((float) ccCluster.clusterNum / ccClusteringCount) << "%" << std::flush;
        }

        for (auto ccDedupAddr : ccClusterDedupAddrs) {
            auto bchAddressScriptBase = blocksci::Address{ccDedupAddr.scriptNum, reprType(ccDedupAddr.type), bch.getAccess()}.getBaseScript();
            if (bchAddressScriptBase.hasBeenSeen()) {
                ++bchAddressCount;
            }

            // determine last usage of current address and update last usage of cluster if needed
            if (addressLastUsageBtc[ccDedupAddr] > ccClusterLastUsageBtc) {
                ccClusterLastUsageBtc = addressLastUsageBtc[ccDedupAddr];
            }

            // check if a tag can be assigned to the current address
            if (tags.find(ccDedupAddr) != tags.end()) {
                ccClusterTagCategories.insert(tags[ccDedupAddr].first);
                std::pair<std::unordered_set<std::string>::iterator, bool> insertResult = ccClusterTags.insert(tags[ccDedupAddr].second);
                if (insertResult.second) {
                    fout_cluster_tags
                        << ccCluster.clusterNum << ","
                        << tags[ccDedupAddr].first << ","
                        << tags[ccDedupAddr].second
                        << "\n";
                }
            }

            // find the cluster in the single-chain clustering that contains the current address
            auto clusterInScClustering = scClustering.getCluster(blocksci::RawAddress{ccDedupAddr.scriptNum, reprType(ccDedupAddr.type)});
            if (clusterInScClustering) {
                uint32_t scClusterSize = clusterInScClustering->getTypeEquivSize();

                // check if sc cluster is the smallest/largest seen so far
                if (scClusterSize < smallestScClusterSize) {
                    smallestScClusterSize = scClusterSize;
                }
                if (scClusterSize > largestScClusterSize) {
                    largestScClusterSize = scClusterSize;
                }

                if (clustersInScData.find(clusterInScClustering->clusterNum) == clustersInScData.end()) {
                    std::unordered_set<std::string> scClusterTagCategories;
                    std::unordered_set<std::string> scClusterTags;
                    uint32_t scClusterLastUsageBtc = 0;

                    // determine tags of single-chain cluster
                    auto scClusterDedupAddrs = clusterInScClustering->getDedupAddresses();
                    for (auto scDedupAddr : scClusterDedupAddrs) {
                        // check if a tag can be assigned to the current address
                        if (tags.find(scDedupAddr) != tags.end()) {
                            scClusterTagCategories.insert(tags[scDedupAddr].first);
                            scClusterTags.insert(tags[scDedupAddr].second);
                        }

                        // determine last usage of current address and update last usage of sc cluster if needed
                        if (addressLastUsageBtc[scDedupAddr] > scClusterLastUsageBtc) {
                            scClusterLastUsageBtc = addressLastUsageBtc[scDedupAddr];
                        }
                    }

                    clustersInScData.insert({clusterInScClustering->clusterNum, std::make_tuple(scClusterSize, scClusterLastUsageBtc, scClusterTagCategories, scClusterTags)});
                }
            }
            else {
                addressesMissingInScClustering++;
            }
        }

        uint32_t clusterCountInScData = clustersInScData.size();

        if (bchAddressCount == 0) {
            btcOnlyCluster = true;
            sameAsOnForkHeight = false;
            btcOnlyClusterCount++;
        }
        else {
            btcOnlyCluster = false;

            // check if the cluster has (not) changed since the BCH fork (block height 478588)
            auto firstCcDedupAddr = *(ccCluster.getDedupAddresses().begin());
            auto firstClusterInScData = forkHeightClustering.getCluster(blocksci::RawAddress{firstCcDedupAddr.scriptNum, reprType(firstCcDedupAddr.type)});
            if (firstClusterInScData) {
                if (ccClusterSize == 1) {
                    auto btcAddressScriptBase = blocksci::Address{firstCcDedupAddr.scriptNum, reprType(firstCcDedupAddr.type), btc.getAccess()}.getBaseScript();
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
                    if (ccClusterSize == firstClusterInScData->getTypeEquivSize()) {
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

        for (const auto& cl : clustersInScData) {
            std::stringstream ccc_cluster_components_ss;
            ccc_cluster_components_ss
                << ccCluster.clusterNum << ","
                << cl.first << ","                 // sc cluster ID
                << std::get<0>(cl.second) << ","   // sc cluster size
                << std::get<1>(cl.second) << ","   // sc cluster last usage
                << "\"" << implode(std::get<2>(cl.second)) << "\"," // "implode" sc cluster tag categories
                << "\"" << implode(std::get<3>(cl.second)) << "\"," // "implode" sc cluster tags
                << std::get<3>(cl.second).size()   // sc cluster tag count
                << "\n";

            fout_cluster_components << ccc_cluster_components_ss.str();

            // only output if ccc-cluster contains several single-chain clusters
            if (clusterCountInScData > 1) {
                fout_cluster_components_relevant << ccc_cluster_components_ss.str();
            }
        }

        std::stringstream ccc_clusters_ss;

        ccc_clusters_ss
            << ccCluster.clusterNum << ","
            << ccClusterSize << ","
            << bchAddressCount << ","
            << btcOnlyCluster << ","
            << sameAsOnForkHeight << ","
            << clusterCountInScData << ","
            << smallestScClusterSize << ","
            << largestScClusterSize << ","
            << (ccClusterSize - largestScClusterSize) << ","
            << ((float) ccClusterSize / largestScClusterSize - 1) << ","
            //<< ccClusterBalance << ","
            << ccClusterLastUsageBtc << ","
            << "\"" << implode(ccClusterTagCategories) << "\","
            << "\"" << implode(ccClusterTags) << "\","
            << ccClusterTags.size()
            << "\n";

        fout_ccc_clusters << ccc_clusters_ss.str();

        // only if ccc-cluster contains several single-chain clusters
        if (clusterCountInScData > 1) {
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
