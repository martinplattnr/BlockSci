//
//  cluster_access.hpp
//  blocksci
//
//  Created by Harry Kalodner on 4/16/18.
//

#ifndef cluster_access_h
#define cluster_access_h

#include "address_info.hpp"
#include "dedup_address_info.hpp"
#include "file_mapper.hpp"

#include <blocksci/core/raw_address.hpp>
#include <blocksci/core/dedup_address.hpp>

#include <range/v3/iterator_range.hpp>

#include <wjfilesystem/path.h>

namespace blocksci {
    template<DedupAddressType::Enum type>
    struct ScriptClusterIndexFile : public FixedSizeFileMapper<uint32_t> {
        using FixedSizeFileMapper<uint32_t>::FixedSizeFileMapper;
    };
    
    class ClusterAccess;
    class DataAccess;
    
    template<blocksci::DedupAddressType::Enum type>
    struct ClusterNumFunctor {
        static uint32_t f(const ClusterAccess *access, uint32_t scriptNum);
    };
    
    class ClusterAccess {
        FixedSizeFileMapper<uint32_t> clusterOffsetFile;
        FixedSizeFileMapper<DedupAddress> clusterScriptsFile;
        
        using ScriptClusterIndexTuple = to_dedup_address_tuple_t<ScriptClusterIndexFile>;
        
        ScriptClusterIndexTuple scriptClusterIndexFiles;
        
        
        friend class Cluster;
        
        template<DedupAddressType::Enum type>
        friend struct ClusterNumFunctor;
        
        template<DedupAddressType::Enum type>
        uint32_t getClusterNumImpl(uint32_t scriptNum) const {
            auto &file = std::get<ScriptClusterIndexFile<type>>(scriptClusterIndexFiles);
            return *file[scriptNum - 1];
        }
        
    public:
        DataAccess &access;
        
        ClusterAccess(const std::string &baseDirectory, DataAccess &access_) :
        clusterOffsetFile((filesystem::path{baseDirectory}/"clusterOffsets").str()),
        clusterScriptsFile((filesystem::path{baseDirectory}/"clusterAddresses").str()),
        scriptClusterIndexFiles(blocksci::apply(DedupAddressType::all(), [&] (auto tag) {
            std::stringstream ss;
            ss << dedupAddressName(tag) << "_cluster_index";
            return (filesystem::path{baseDirectory}/ss.str()).str();
        })),
        access(access_)  {
            if (!(filesystem::path{baseDirectory}/"clusterAddresses.dat").exists()) {
                throw std::runtime_error("Cluster data not found");
            }
        }
        
        static std::string offsetFilePath(const std::string &baseDirectory) {
            return (filesystem::path{baseDirectory}/"clusterOffsets.dat").str();
        }
        
        static std::string addressesFilePath(const std::string &baseDirectory) {
            return (filesystem::path{baseDirectory}/"clusterAddresses.dat").str();
        }
        
        static std::string typeIndexFilePath(const std::string &baseDirectory, DedupAddressType::Enum type) {
            filesystem::path base{baseDirectory};
            std::stringstream ss;
            ss << dedupAddressName(type) << "_cluster_index.dat";
            return (base/ss.str()).str();
        }

        ranges::optional<uint32_t> getClusterNum(const RawAddress &address) const {
            static auto table = blocksci::make_dynamic_table<DedupAddressType, ClusterNumFunctor>();
            auto index = static_cast<size_t>(dedupType(address.type));
            auto clusterNum = table.at(index)(this, address.scriptNum);
            if (clusterNum != std::numeric_limits<uint32_t>::max()) {
                return clusterNum;
            }
            // when using multi-chain clustering with reduceToChain, the given address may not be found in the index(es)
            return ranges::nullopt;
        }
        
        uint32_t getClusterSize(uint32_t clusterNum) const {
            auto clusterOffset = *clusterOffsetFile[clusterNum];
            auto clusterSize = clusterOffset;
            if (clusterNum > 0) {
                clusterSize -= *clusterOffsetFile[clusterNum - 1];
            }
            return clusterSize;
        }
        
        uint32_t clusterCount() const {
            return static_cast<uint32_t>(clusterOffsetFile.size()) - 1;
        }
        
        ranges::iterator_range<const blocksci::DedupAddress *> getClusterScripts(uint32_t clusterNum) const {
            auto nextClusterOffset = *clusterOffsetFile[clusterNum];
            uint32_t clusterOffset = 0;
            if (clusterNum > 0) {
                clusterOffset = *clusterOffsetFile[clusterNum - 1];
            }
            auto clusterSize = nextClusterOffset - clusterOffset;
            
            auto firstAddressOffset = clusterScriptsFile[clusterOffset];
            
            return ranges::make_iterator_range(firstAddressOffset, firstAddressOffset + clusterSize);
        }
        
        std::vector<uint32_t> getClusterSizes() const {
            auto tot = clusterCount();
            std::vector<uint32_t> clusterSizes;
            clusterSizes.resize(tot);
            
            clusterSizes[tot - 1] = *clusterOffsetFile[tot - 1];
            for (uint32_t i = 2; i <= tot; i++) {
                clusterSizes[tot - i] = *clusterOffsetFile[tot - i];
                clusterSizes[(tot - i) + 1] -= clusterSizes[tot - i];
            }
            return clusterSizes;
        }
    };
    
    template<blocksci::DedupAddressType::Enum type>
    uint32_t ClusterNumFunctor<type>::f(const ClusterAccess *access, uint32_t scriptNum) {
        return access->getClusterNumImpl<type>(scriptNum);
    }
    
} // namespace blocksci

#endif /* cluster_access_h */
