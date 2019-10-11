//
//  address_state.hpp
//  blocksci
//
//  Created by Harry Kalodner on 9/10/17.
//
//

#ifndef address_state_hpp
#define address_state_hpp

#include "bloom_filter.hpp"
#include "parser_fwd.hpp"
#include "serializable_map.hpp"
#include "hash_index_creator.hpp"

#include <internal/dedup_address_info.hpp>
#include <internal/bitcoin_uint256_hex.hpp>

#include <memory>

enum class AddressLocation {
    MultiUseMap, // map of addresses that are used multiple times
    RocksDb,     // DB that contains a hash of all script identifiers (pubkey, scripthash etc.)
    NotFound     // address was not found in any of the 3 data structures (bloom filter, multi-use map, rocksdb)
};

template<blocksci::AddressType::Enum type>
struct RawAddressInfo {
    blocksci::uint160 hash;
    AddressLocation location;
    uint32_t addressNum;
};

template<blocksci::AddressType::Enum type>
struct NonDudupAddressInfo {
    uint32_t addressNum;
};

template<blocksci::DedupAddressType::Enum>
constexpr int startingCount = 0;
template<>
constexpr int startingCount<blocksci::DedupAddressType::PUBKEY> = 600'000'000;
template<>
constexpr int startingCount<blocksci::DedupAddressType::SCRIPTHASH> = 100'000'000;
template<>
constexpr int startingCount<blocksci::DedupAddressType::MULTISIG> = 100'000'000;


/** This class is used to detect if an address has been seen before, and if yes, which scriptNum was assigned to it.
 *
 *  It uses 3 data layers, the fastest is accessed first, then the second-fastest if needed, etc.
 *
 *  1) Bloom filter (fastest; shared between forks):
 *      A bloom filter is queried to reliably detect if the address has never been seen.
 *
 *  todo-fork: the multi-use map could also be shared between forks
 *  2) Multi-use map (slower; not shared between forks):
 *      If the bloom filter returns that the address may have been seen, the multi-use map is queried.
 *      It stores all addresses that have been used multiple times, as it is likely that addresses are reused again if they are used twice.
 *
 *  3) RocksDB database (slowest; shared between forks):
 *      If 2) does not return a result, a RocksDB database is queried.
 */
class AddressState {
    static constexpr auto AddressFalsePositiveRate = .05;
    
    template<blocksci::DedupAddressType::Enum scriptType>
    class AddressMap : public SerializableMap<blocksci::uint160, uint32_t>  {
    public:
        static constexpr auto type = scriptType;
        AddressMap() : SerializableMap(blocksci::uint160S("FFFFFFFFFFFFFFFFFFFF"), blocksci::uint160S("AAAAAAAAAAAAAAAAAA")) {}
    };
    
    template<blocksci::DedupAddressType::Enum scriptType>
    class AddressBloomFilter : public BloomFilter  {
    public:
        static constexpr auto type = scriptType;
        AddressBloomFilter(const filesystem::path &path) : BloomFilter(filesystem::path(path.str() + dedupAddressName(type)).str(), startingCount<scriptType>, AddressFalsePositiveRate)  {}
    };

    template<blocksci::DedupAddressType::Enum scriptType>
    using AddressBloomFilterPointer = std::unique_ptr<AddressBloomFilter<scriptType>>;

    /** path to the parser/addresses directory of the current chain, when working with forks, otherwise same as rootDirectory */
    filesystem::path localDirectory;

    /** path to the parser/addresses directory of the root chain, when working with forks, otherwise same as localDirectory */
    filesystem::path rootDirectory;

    HashIndexCreator &db;
    
    using AddressMapTuple = blocksci::to_dedup_address_tuple_t<AddressMap>;
    using AddressBloomFilterTuple = blocksci::to_dedup_address_tuple_t<AddressBloomFilterPointer>;
    
    AddressMapTuple multiAddressMaps;
    AddressBloomFilterTuple addressBloomFilters;
    
    mutable long bloomNegativeCount = 0;
    mutable long multiCount = 0;
    mutable long dbCount = 0;
    mutable long bloomFPCount = 0;
    
    
    std::vector<uint32_t> scriptIndexes;
    
    template<blocksci::AddressType::Enum type>
    void reloadBloomFilter(int sizeIncreaseRatio) {
        auto &addressBloomFilter = std::get<AddressBloomFilterPointer<dedupType(type)>>(addressBloomFilters);
        addressBloomFilter->reset(addressBloomFilter->getMaxItems() * sizeIncreaseRatio, addressBloomFilter->getFPRate());
        
        db.clearAddressCache<blocksci::DedupAddressInfo<dedupType(type)>::reprType>();
        
        RANGES_FOR(auto item, db.db.getAddressRange<blocksci::DedupAddressInfo<dedupType(type)>::reprType>()) {
            addressBloomFilter->add(item.second);
        }
    }
    
    void reloadBloomFilters() {
        reloadBloomFilter<blocksci::AddressType::PUBKEYHASH>(1);
        reloadBloomFilter<blocksci::AddressType::SCRIPTHASH>(1);
        reloadBloomFilter<blocksci::AddressType::MULTISIG>(1);
    }
    
public:
    AddressState(filesystem::path localDirectory, filesystem::path rootDirectory, HashIndexCreator &hashDb);
    AddressState(const AddressState &) = delete;
    AddressState &operator=(const AddressState &) = delete;
    AddressState(AddressState &&) = delete;
    AddressState &operator=(AddressState &&) = delete;
    ~AddressState();

    // used if DedupAddressInfo<type>::equived = false (DedupAddressType::NONSTANDARD, DedupAddressType::NULL_DATA, DedupAddressType::WITNESS_UNKNOWN)
    template<blocksci::AddressType::Enum type, std::enable_if_t<!blocksci::DedupAddressInfo<dedupType(type)>::equived, int> = 0>
    NonDudupAddressInfo<type> findAddress(const ScriptOutputData<type> &) {
        uint32_t scriptNum = getNewAddressIndex(dedupType(type));
        return NonDudupAddressInfo<type>{scriptNum};
    }
    
    template<blocksci::AddressType::Enum type>
    std::pair<uint32_t, bool> resolveAddress(const NonDudupAddressInfo<type> &addressInfo) {
        return std::make_pair(addressInfo.addressNum, true);
    }

    // check if an address exists already
    template<blocksci::AddressType::Enum type, std::enable_if_t<blocksci::DedupAddressInfo<dedupType(type)>::equived, int> = 0>
    RawAddressInfo<type> findAddress(const ScriptOutputData<type> &data) {
        auto hash = data.getHash();
        auto &addressBloomFilter = std::get<AddressBloomFilterPointer<dedupType(type)>>(addressBloomFilters);
        if (!addressBloomFilter->possiblyContains(hash)) {
            // Address has definitely never been seen
            bloomNegativeCount++;
            return {hash, AddressLocation::NotFound, 0};
        }
        
        {
            auto &multiAddressMap = std::get<AddressMap<dedupType(type)>>(multiAddressMaps);
            auto it = multiAddressMap.find(hash);
            if (it != multiAddressMap.end()) {
                multiCount++;
                return {hash, AddressLocation::MultiUseMap, it->second};
            }
        }
        
        ranges::optional<uint32_t> destNum = db.lookupAddress<blocksci::DedupAddressInfo<dedupType(type)>::reprType>(hash);
        if (destNum) {
            dbCount++;
            return {hash, AddressLocation::RocksDb, *destNum};
        } else {
            bloomFPCount++;
            // We must have had a false positive
            return {hash, AddressLocation::NotFound, 0};
        }
    }
    
    // Bool is true if address is new
    // called by ScriptOutput.resolve(): first calls findAddress, then calls resolveAddress with the result of findAddress
    template<blocksci::AddressType::Enum type>
    std::pair<uint32_t, bool> resolveAddress(const RawAddressInfo<type> &addressInfo) {
        bool existingAddress = false;
        switch (addressInfo.location) {
            case AddressLocation::RocksDb: {
                // address was found in RocksDb, so it occurs at least twice -> add it to multiAddressMaps
                auto &multiAddressMap = std::get<AddressMap<dedupType(type)>>(multiAddressMaps);
                multiAddressMap.add(addressInfo.hash, addressInfo.addressNum);
                existingAddress = true;
                break;
            }
            case AddressLocation::MultiUseMap: {
                existingAddress = true;
                break;
            }
            case AddressLocation::NotFound: {
                existingAddress = false;
                break;
            }
        }
        
        uint32_t addressNum = addressInfo.addressNum;
        if (!existingAddress) {
            addressNum = getNewAddressIndex(dedupType(type));
            auto &addressBloomFilter = std::get<AddressBloomFilterPointer<dedupType(type)>>(addressBloomFilters);
            addressBloomFilter->add(addressInfo.hash);
            db.addAddress<blocksci::DedupAddressInfo<dedupType(type)>::reprType>(addressInfo.hash, addressNum);
            if (addressBloomFilter->isFull()) {
                reloadBloomFilter<type>(2);
            }
        }
        return std::make_pair(addressNum, !existingAddress);
    }
    
    uint32_t getNewAddressIndex(blocksci::DedupAddressType::Enum type);
    
    // Called before reseting index
    void rollback(const blocksci::State &state);
    
    // Called after resetting index
    void reset(const blocksci::State &state);
};


#endif /* address_state_hpp */
