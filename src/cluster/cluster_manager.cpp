//
//  cluster_manager.cpp
//  blocksci
//
//  Created by Harry Kalodner on 7/6/17.
//
//

#include <blocksci/cluster/cluster_manager.hpp>
#include <blocksci/cluster/cluster.hpp>

#include <blocksci/chain/blockchain.hpp>
#include <blocksci/chain/input.hpp>
#include <blocksci/chain/range_util.hpp>
#include <blocksci/core/dedup_address.hpp>
#include <blocksci/heuristics/change_address.hpp>
#include <blocksci/heuristics/tx_identification.hpp>
#include <blocksci/scripts/scripthash_script.hpp>

#include <internal/address_info.hpp>
#include <internal/cluster_access.hpp>
#include <internal/data_access.hpp>
#include <internal/progress_bar.hpp>
#include <internal/script_access.hpp>

#include <dset/dset.h>

#include <wjfilesystem/path.h>

#include <range/v3/view/iota.hpp>
#include <range/v3/range_for.hpp>
#include <fstream>
#include <future>
#include <map>

namespace {
    template <typename Job>
    void segmentWork(uint32_t start, uint32_t end, uint32_t segmentCount, Job job) {
        uint32_t total = end - start;
        
        // Don't partition over threads if there are less items than segment count
        if (total < segmentCount) {
            for (uint32_t i = start; i < end; ++i) {
                job(i);
            }
            return;
        }
        
        auto segmentSize = total / segmentCount;
        auto segmentsRemaining = total % segmentCount;
        std::vector<std::pair<uint32_t, uint32_t>> segments;
        uint32_t i = 0;
        while(i < total) {
            uint32_t startSegment = i;
            i += segmentSize;
            if (segmentsRemaining > 0) {
                i += 1;
                segmentsRemaining--;
            }
            uint32_t endSegment = i;
            segments.emplace_back(startSegment + start, endSegment + start);
        }
        std::vector<std::thread> threads;
        for (uint32_t i = 0; i < segmentCount - 1; i++) {
            auto segment = segments[i];
            threads.emplace_back([segment, &job](){
                for (uint32_t i = segment.first; i < segment.second; i++) {
                    job(i);
                }
            });
        }
        
        auto segment = segments.back();
        for (uint32_t i = segment.first; i < segment.second; i++) {
            job(i);
        }
        
        for (auto &thread : threads) {
            thread.join();
        }
    }
}

namespace blocksci {
    ClusterManager::ClusterManager(const std::string &baseDirectory, DataAccess &access_) : access(std::make_unique<ClusterAccess>(baseDirectory, access_)), clusterCount(access->clusterCount()) {}
    
    ClusterManager::ClusterManager(ClusterManager && other) = default;
    
    ClusterManager &ClusterManager::operator=(ClusterManager && other) = default;
    
    ClusterManager::~ClusterManager() = default;
    
    Cluster ClusterManager::getCluster(const Address &address) const {
        return Cluster(access->getClusterNum(RawAddress{address.scriptNum, address.type}), *access);
    }
    
    ranges::any_view<Cluster, ranges::category::random_access | ranges::category::sized> ClusterManager::getClusters() const {
        return ranges::view::ints(0u, clusterCount)
        | ranges::view::transform([&](uint32_t clusterNum) { return Cluster(clusterNum, *access); });
    }
    
    ranges::any_view<TaggedCluster> ClusterManager::taggedClusters(const std::unordered_map<Address, std::string> &tags) const {
        return getClusters() | ranges::view::transform([tags](Cluster && cluster) -> ranges::optional<TaggedCluster> {
            return cluster.getTaggedUnsafe(tags);
        }) | flatMapOptionals();
    }
    
    struct AddressDisjointSets {
        DisjointSets disjoinSets;
        std::unordered_map<DedupAddressType::Enum, uint32_t> addressStarts;
        
        AddressDisjointSets(uint32_t totalSize, std::unordered_map<DedupAddressType::Enum, uint32_t> addressStarts_) : disjoinSets{totalSize}, addressStarts{std::move(addressStarts_)} {}
        
        uint32_t size() const {
            return disjoinSets.size();
        }

        auto addressIndex(const Address address) const {
            return addressStarts.at(dedupType(address.type)) + address.scriptNum - 1;
        }
        
        void link_addresses(const Address &address1, const Address &address2) {
            //auto firstAddressIndex = addressStarts.at(dedupType(address1.type)) + address1.scriptNum - 1;
            //auto secondAddressIndex = addressStarts.at(dedupType(address2.type)) + address2.scriptNum - 1;
            auto firstAddressIndex = addressIndex(address1);
            auto secondAddressIndex = addressIndex(address2);
            disjoinSets.unite(firstAddressIndex, secondAddressIndex);
        }

        void link_clusters(const uint32_t cluster1Id, const uint32_t cluster2Id) {
            disjoinSets.unite(cluster1Id, cluster2Id);
        }
        
        void resolveAll() {
            segmentWork(0, disjoinSets.size(), 8, [&](uint32_t index) {
                disjoinSets.find(index);
            });
        }
        
        uint32_t find(uint32_t index) {
            return disjoinSets.find(index);
        }
    };
    
    template <typename ChangeFunc>
    std::vector<std::pair<Address, Address>> processTransaction(const Transaction &tx, ChangeFunc && changeHeuristic,
                                                                bool ignoreCoinJoin) {
        std::vector<std::pair<Address, Address>> pairsToUnion;
        
        if (!tx.isCoinbase() && (!ignoreCoinJoin || !heuristics::isCoinjoin(tx))) {
            auto inputs = tx.inputs();
            auto firstAddress = inputs[0].getAddress();
            for (uint16_t i = 1; i < inputs.size(); i++) {
                pairsToUnion.emplace_back(firstAddress, inputs[i].getAddress());
            }
            
            RANGES_FOR(auto change, std::forward<ChangeFunc>(changeHeuristic)(tx)) {
                pairsToUnion.emplace_back(change.getAddress(), firstAddress);
            }
        }
        return pairsToUnion;
    }
    
    void linkScripthashNested(DataAccess &access, AddressDisjointSets &ds) {
        auto scriptHashCount = access.getScripts().scriptCount(DedupAddressType::SCRIPTHASH);

        auto progressBar = makeProgressBar(scriptHashCount / 8, [=]() {});
        uint32_t maxScriptNumOfFirstThread = scriptHashCount / 8;
        uint32_t scriptCountOfFirstThread = 0;
        segmentWork(1, scriptHashCount + 1, 8, [&ds, &access, &progressBar, &maxScriptNumOfFirstThread, &scriptCountOfFirstThread](uint32_t index) {
            Address pointer(index, AddressType::SCRIPTHASH, access);
            script::ScriptHash scripthash{index, access};
            auto wrappedAddress = scripthash.getWrappedAddress();
            if (wrappedAddress) {
                ds.link_addresses(pointer, *wrappedAddress);
            }
            if (index < maxScriptNumOfFirstThread) {
                scriptCountOfFirstThread++;
                progressBar.update(scriptCountOfFirstThread);
            }
        });
    }

    /** returns a vector that stores the clusterid for every scriptNum, indexed by scriptNum */
    template <typename ChangeFunc>
    std::vector<uint32_t> createClusters(std::vector<BlockRange> &chains, std::unordered_map<DedupAddressType::Enum, uint32_t> addressStarts, uint32_t totalScriptCount, ChangeFunc && changeHeuristic, bool ignoreCoinJoin) {

        std::cout << "Preparing data structure for clustering" << std::endl;
        AddressDisjointSets ds(totalScriptCount, std::move(addressStarts));
        
        auto &access = chains[0].getAccess();

        std::cout << "Linking nested script-hash addresses" << std::endl;
        linkScripthashNested(access, ds);

        // todo: add user option to log merge events or remove this feature entirely
        std::ofstream logfile;

        auto extract = [&](const BlockRange &blocks, int threadNum) {
            auto progressThread = static_cast<int>(0); // todo: revert, temporary change to show progress bar if there is just 1 thread
            auto progressBar = makeProgressBar(blocks.endTxIndex() - blocks.firstTxIndex(), [=]() {});
            if (threadNum != progressThread) {
                progressBar.setSilent();
            }
            uint32_t txNum = 0;
            for (auto block : blocks) {
                for (auto tx : block) {
                    auto pairs = processTransaction(tx, changeHeuristic, ignoreCoinJoin);
                    for (auto &pair : pairs) {
                        uint32_t cluster1 = ds.find(ds.addressIndex(pair.first));
                        uint32_t cluster2 = ds.find(ds.addressIndex(pair.second));
                        if (cluster2 > cluster1) {
                            std::swap(cluster1, cluster2);
                        }
                        if (cluster1 != cluster2) {
                            // log (block, block.timestamp, txNum, cluster1, cluster2)
                            logfile << block.height() << ";" << block.timestamp() << ";" << tx.txNum << ";" << cluster1 << ";" << cluster2 << std::endl;
                        }

                        ds.link_clusters(cluster1, cluster2);

                        //ds.link_addresses(pair.first, pair.second);
                    }
                    progressBar.update(txNum);
                    txNum++;
                }
            }
            return 0;
        };

        for (auto chain : chains) {
            std::cout << "Clustering using " << chain.getAccess().config.chainConfig.coinName << " data" << std::endl;
            logfile.open ("cluster_log_" + chain.getAccess().config.chainConfig.coinName + ".txt");
            chain.mapReduce<int>(extract, [](int &a,int &) -> int & {return a;});
            logfile.close();
        }

        std::cout << "Performing post-processing: resolving cluster nums for every address" << std::endl;

        // this step is not necessary, but can be processed in parallel and speeds up the find() operations in code below
        ds.resolveAll();
        
        std::vector<uint32_t> parents;
        parents.reserve(ds.size());
        for (uint32_t i = 0; i < totalScriptCount; i++) {
            parents.push_back(ds.find(i));
        }
        return parents;
    }

    /** remaps cluster IDs, count number of clusters/addresses and return (addressCount, clusterCount) tuple */
    std::tuple<uint32_t, uint32_t> remapClusterIds(std::vector<uint32_t> &parents, std::map<uint32_t, DedupAddressType::Enum> typeIndexes, ranges::optional<const BlockRange&> reduceToChain) {
        std::cout << "Performing post-processing: remapping cluster IDs";
        if (reduceToChain) {
            std::cout << " and reducing to chain " << reduceToChain->getAccess().config.chainConfig.coinName;
        }
        std::cout << std::endl;

        auto progressBar = makeProgressBar(parents.size(), [=]() {});

        uint32_t placeholder = std::numeric_limits<uint32_t>::max();
        std::vector<uint32_t> newClusterIds(parents.size(), placeholder);

        uint32_t addressCount = 0;
        uint32_t clusterCount = 0;
        uint32_t parentIndex = 0;
        uint32_t excludedAddresses = 0;
        bool seenAddress = false;
        for (uint32_t &clusterNum : parents) {
            if (reduceToChain) {
                // if reduceToChain is active, check if current address hasBeenSeen on the chain to reduce to
                auto it = typeIndexes.upper_bound(parentIndex);
                it--;
                uint32_t addressNum = parentIndex - it->first + 1;
                auto addressType = it->second;
                seenAddress = reduceToChain->getAccess().scripts->getScriptHeader(addressNum, addressType)->hasBeenSeen();
            }

            if ( ! reduceToChain || seenAddress) {
                addressCount++;
                uint32_t &clusterId = newClusterIds[clusterNum];
                if (clusterId == placeholder) {
                    clusterId = clusterCount;
                    clusterCount++;
                }
                clusterNum = clusterId;
            }
            else {
                // reducing to single chain is active and address has not been seen -> exclude address by setting clusterNum in parents to max(uint32_t)
                clusterNum = std::numeric_limits<uint32_t>::max();
                excludedAddresses++;
            }

            parentIndex++;
            progressBar.update(parentIndex);
        }

        std::cout.setf(std::ios::fixed,std::ios::floatfield);
        std::cout.precision(2);
        std::cout << std::endl << "Reduce result: excluded " << excludedAddresses << " out of " << parents.size() << " addresses (" << (excludedAddresses / parents.size() * 100) << "%)" << std::endl;
        
        return std::make_tuple(addressCount, clusterCount);
    }
    
    void recordOrderedAddresses(const std::vector<uint32_t> &parent, std::vector<uint32_t> &clusterPositions, const std::unordered_map<DedupAddressType::Enum, uint32_t> &scriptStarts, std::ofstream &clusterAddressesFile, uint32_t addressCount) {

        std::map<uint32_t, DedupAddressType::Enum> typeIndexes;
        for (auto &pair : scriptStarts) {
            auto it = typeIndexes.find(pair.second);
            if(it != typeIndexes.end()) {
                // If an address type is not used, skip to the next one
                it->second = std::max(it->second, pair.first);
            } else {
                typeIndexes[pair.second] = pair.first;
            }
        }
        
        std::vector<DedupAddress> orderedScripts;
        orderedScripts.resize(addressCount);
        
        for (uint32_t i = 0; i < parent.size(); i++) {
            if (parent[i] == std::numeric_limits<uint32_t>::max()) {
                // address was excluded during reducing to single chain -> skip address
                continue;
            }

            uint32_t &j = clusterPositions[parent[i]];
            auto it = typeIndexes.upper_bound(i);
            it--;
            uint32_t addressNum = i - it->first + 1;
            auto addressType = it->second;
            orderedScripts[j] = DedupAddress(addressNum, addressType);
            j++;
        }
        
        clusterAddressesFile.write(reinterpret_cast<char *>(orderedScripts.data()), static_cast<long>(sizeof(DedupAddress) * orderedScripts.size()));
    }
    
    void prepareClusterDataLocation(const std::string &outputPath, bool overwrite) {
        auto outputLocation = filesystem::path{outputPath};
        
        std::string offsetFile = ClusterAccess::offsetFilePath(outputPath);
        std::string addressesFile = ClusterAccess::addressesFilePath(outputPath);
        std::vector<std::string> clusterIndexPaths;
        clusterIndexPaths.resize(DedupAddressType::size);
        for (auto dedupType : DedupAddressType::allArray()) {
            clusterIndexPaths[static_cast<size_t>(dedupType)] = ClusterAccess::typeIndexFilePath(outputPath, dedupType);
        }
        
        std::vector<std::string> allPaths = clusterIndexPaths;
        allPaths.push_back(offsetFile);
        allPaths.push_back(addressesFile);
        
        // Prepare cluster folder or fail
        auto outputLocationPath = filesystem::path{outputLocation};
        if (outputLocationPath.exists()) {
            if (!outputLocationPath.is_directory()) {
                throw std::runtime_error{"Path must be to a directory, not a file"};
            }
            if (!overwrite) {
                for (auto &path : allPaths) {
                    auto filePath = filesystem::path{path};
                    if (filePath.exists()) {
                        std::stringstream ss;
                        ss << "Overwrite is off, but " << filePath << " exists already";
                        throw std::runtime_error{ss.str()};
                    }
                }
            } else {
                for (auto &path : allPaths) {
                    auto filePath = filesystem::path{path};
                    if (filePath.exists()) {
                        filePath.remove_file();
                    }
                }
            }
        } else {
            if(!filesystem::create_directory(outputLocationPath)) {
                std::stringstream ss;
                ss << "Cannot create directory at path " << outputLocationPath;
                throw std::runtime_error(ss.str());
            }
        }
    }
    
    void serializeClusterData(const ScriptAccess &scripts, const std::string &outputPath, const std::vector<uint32_t> &parent, const std::unordered_map<DedupAddressType::Enum, uint32_t> &scriptStarts, uint32_t addressCount, uint32_t clusterCount) {
        std::cout << "Saving cluster data to files" << std::endl;

        auto outputLocation = filesystem::path{outputPath};
        std::string offsetFile = ClusterAccess::offsetFilePath(outputPath);
        std::string addressesFile = ClusterAccess::addressesFilePath(outputPath);
        std::vector<std::string> clusterIndexPaths;
        clusterIndexPaths.resize(DedupAddressType::size);
        for (auto dedupType : DedupAddressType::allArray()) {
            clusterIndexPaths[static_cast<size_t>(dedupType)] = ClusterAccess::typeIndexFilePath(outputPath, dedupType);
        }

        // Generate cluster files
        std::vector<uint32_t> clusterPositions;
        clusterPositions.resize(clusterCount + 1);
        for (auto parentId : parent) {
            if (parentId != std::numeric_limits<uint32_t>::max()) {
                clusterPositions[parentId + 1]++;
            }
        }
        
        for (size_t i = 1; i < clusterPositions.size(); i++) {
            clusterPositions[i] += clusterPositions[i-1];
        }
        std::ofstream clusterAddressesFile(addressesFile, std::ios::binary);
        auto recordOrdered = std::async(std::launch::async, recordOrderedAddresses, parent, std::ref(clusterPositions), scriptStarts, std::ref(clusterAddressesFile), addressCount);
        
        segmentWork(0, DedupAddressType::size, DedupAddressType::size, [&](uint32_t index) {
            auto type = static_cast<DedupAddressType::Enum>(index);
            uint32_t startIndex = scriptStarts.at(type);
            uint32_t totalCount = scripts.scriptCount(type);
            std::ofstream file{clusterIndexPaths[index], std::ios::binary};
            file.write(reinterpret_cast<const char *>(parent.data() + startIndex), sizeof(uint32_t) * totalCount);
        });
        
        recordOrdered.get();
        
        std::ofstream clusterOffsetFile(offsetFile, std::ios::binary);
        clusterOffsetFile.write(reinterpret_cast<char *>(clusterPositions.data()), static_cast<long>(sizeof(uint32_t) * clusterPositions.size()));
    }

    ranges::optional<const BlockRange&> getReduceToChain(std::vector<BlockRange> &chains, ChainId::Enum reduceTo) {
        if (reduceTo != ChainId::UNSPECIFIED) {
            for (auto &chain : chains) {
                if (chain.getAccess().config.chainConfig.chainId == reduceTo) {
                    return chain;
                }
            }
            throw std::runtime_error("The reduce chain ID was not found in the given chains");
        }
        return ranges::nullopt;
    }
    
    template <typename ChangeFunc>
    ClusterManager createClusteringImpl(std::vector<BlockRange> &chains, ChangeFunc && changeHeuristic, const std::string &outputPath, bool overwrite, ChainId::Enum reduceToChainId, bool ignoreCoinJoin) {
        prepareClusterDataLocation(outputPath, overwrite);
        
        // Perform clustering
        if (chains.size() == 1) {
            std::cout << "Creating single-chain clustering" << std::endl;
        }
        else {
            std::cout << "Creating " << (reduceToChainId == ChainId::UNSPECIFIED ? "multi-chain" : ChainId::getName(reduceToChainId)) << " clustering based on " << chains.size() << " chain(s)" << std::endl << std::endl;
        }

        auto &rootChain = chains[0];

        if (chains.size() > 1 && rootChain.getAccess().config.parentDataConfiguration != nullptr) {
            throw std::invalid_argument("The first provided chain must be a root chain.");
        }

        auto &scripts = rootChain.getAccess().getScripts();
        size_t totalScriptCount = scripts.totalAddressCount();

        auto rootScriptsDirectory = rootChain.getAccess().config.rootScriptsDirectory();
        for (auto &chain : chains) {
            if (chain.getAccess().config.rootScriptsDirectory() != rootScriptsDirectory) {
                throw std::invalid_argument("The provided chains do not belong to the same root chain.");
            }
        }

        auto reduceToChain = getReduceToChain(chains, reduceToChainId);
        
        std::unordered_map<DedupAddressType::Enum, uint32_t> scriptStarts;
        {
            std::vector<uint32_t> starts(DedupAddressType::size);
            for (size_t i = 0; i < DedupAddressType::size; i++) {
                if (i > 0) {
                    starts[i] = scripts.scriptCount(static_cast<DedupAddressType::Enum>(i - 1)) + starts[i - 1];
                }
                scriptStarts[static_cast<DedupAddressType::Enum>(i)] = starts[i];
            }
        }

        std::map<uint32_t, DedupAddressType::Enum> typeIndexes;
        for (auto &pair : scriptStarts) {
            auto it = typeIndexes.find(pair.second);
            if(it != typeIndexes.end()) {
                // If an address type is not used, skip to the next one
                it->second = std::max(it->second, pair.first);
            } else {
                typeIndexes[pair.second] = pair.first;
            }
        }

        auto parent = createClusters(chains, scriptStarts, static_cast<uint32_t>(totalScriptCount), std::forward<ChangeFunc>(changeHeuristic), ignoreCoinJoin);
        uint32_t addressCount, clusterCount;
        std::tie(addressCount, clusterCount) = remapClusterIds(parent, typeIndexes, reduceToChain);
        serializeClusterData(scripts, outputPath, parent, scriptStarts, addressCount, clusterCount);

        std::cout << "Finished clustering with " << addressCount << " addresses in " << clusterCount << " clusters" << std::endl;

        return {filesystem::path{outputPath}.str(), chains[0].getAccess()};
    }
    
    ClusterManager ClusterManager::createClustering(std::vector<BlockRange> &chains, const heuristics::ChangeHeuristic &changeHeuristic, const std::string &outputPath, bool overwrite, ChainId::Enum reduceTo, bool ignoreCoinJoin) {
        
        auto changeHeuristicL = [&changeHeuristic](const Transaction &tx) -> ranges::any_view<Output> {
            return changeHeuristic(tx);
        };
        
        return createClusteringImpl(chains, changeHeuristicL, outputPath, overwrite, reduceTo, ignoreCoinJoin);
    }
    
    ClusterManager ClusterManager::createClustering(std::vector<BlockRange> &chains, const std::function<ranges::any_view<Output>(const Transaction &tx)> &changeHeuristic, const std::string &outputPath, bool overwrite, ChainId::Enum reduceTo, bool ignoreCoinJoin) {
        return createClusteringImpl(chains, changeHeuristic, outputPath, overwrite, reduceTo, ignoreCoinJoin);
    }

    ClusterManager ClusterManager::createClustering(BlockRange &chain, const heuristics::ChangeHeuristic &changeHeuristic, const std::string &outputPath, bool overwrite, ChainId::Enum reduceTo, bool ignoreCoinJoin) {

        auto changeHeuristicL = [&changeHeuristic](const Transaction &tx) -> ranges::any_view<Output> {
            return changeHeuristic(tx);
        };

        std::vector<BlockRange> chains;
        chains.push_back(chain);

        return createClusteringImpl(chains, changeHeuristicL, outputPath, overwrite, reduceTo, ignoreCoinJoin);
    }

    ClusterManager ClusterManager::createClustering(BlockRange &chain, const std::function<ranges::any_view<Output>(const Transaction &tx)> &changeHeuristic, const std::string &outputPath, bool overwrite, ChainId::Enum reduceTo, bool ignoreCoinJoin) {
        std::vector<BlockRange> chains;
        chains.push_back(chain);
        return createClusteringImpl(chains, changeHeuristic, outputPath, overwrite, reduceTo, ignoreCoinJoin);
    }
} // namespace blocksci


