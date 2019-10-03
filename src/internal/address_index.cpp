//
//  address_index.cpp
//  blocksci_address_index
//
//  Created by Harry Kalodner on 7/9/17.
//
//

#define BLOCKSCI_WITHOUT_SINGLETON

#include "address_index.hpp"
#include "address_info.hpp"
#include "column_iterator.hpp"
#include "dedup_address_info.hpp"
#include "memory_view.hpp"

#include <blocksci/core/inout_pointer.hpp>
#include <blocksci/core/raw_address.hpp>
#include <blocksci/core/dedup_address.hpp>

#include <range/v3/view/transform.hpp>
#include <range/v3/range_for.hpp>

#include <endian/big_endian.hpp>

#include <sstream>

namespace blocksci {
    
    AddressIndex::AddressIndex(const filesystem::path &path, bool readonly) {
        rocksdb::Options options;
        // Optimize RocksDB. This is the easiest way to get RocksDB to perform well
        options.IncreaseParallelism();
        options.OptimizeLevelStyleCompaction();
        // Create the DB if it's not already present
        options.create_if_missing = true;
        options.create_missing_column_families = true;
        

        // Initialize RocksDB column families
        std::vector<rocksdb::ColumnFamilyDescriptor> columnDescriptors;
        blocksci::for_each(AddressType::all(), [&](auto tag) {
            std::stringstream ss;
            ss << addressName(tag) << "_output";
            columnDescriptors.emplace_back(ss.str(), rocksdb::ColumnFamilyOptions{});
        });
        blocksci::for_each(AddressType::all(), [&](auto tag) {
            std::stringstream ss;
            ss << addressName(tag) << "_nested";
            columnDescriptors.emplace_back(ss.str(), rocksdb::ColumnFamilyOptions{});
        });
        columnDescriptors.emplace_back(rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions());
        
        rocksdb::DB *dbPtr;
        std::vector<rocksdb::ColumnFamilyHandle *> columnHandlePtrs;
        if (readonly) {
            rocksdb::Status s = rocksdb::DB::OpenForReadOnly(options, path.str().c_str(), columnDescriptors, &columnHandlePtrs, &dbPtr);
            if (!s.ok()) {
                throw std::runtime_error{"Could not open address index with error: " + std::string{s.getState()}};
            }
        } else {
            rocksdb::Status s = rocksdb::DB::Open(options, path.str().c_str(), columnDescriptors, &columnHandlePtrs, &dbPtr);
            if (!s.ok()) {
                throw std::runtime_error{"Could not open address index with error: " + std::string{s.getState()}};
            }
        }
        db = std::unique_ptr<rocksdb::DB>(dbPtr);
        for (auto handle : columnHandlePtrs) {
            columnHandles.emplace_back(std::unique_ptr<rocksdb::ColumnFamilyHandle>(handle));
        }
    }
    
    AddressIndex::~AddressIndex() = default;
    
    void AddressIndex::compactDB() {
        for (auto &column : columnHandles) {
            db->CompactRange(rocksdb::CompactRangeOptions{}, column.get(), nullptr, nullptr);
        }
    }
    
    const std::unique_ptr<rocksdb::ColumnFamilyHandle> &AddressIndex::getOutputColumn(AddressType::Enum type) const {
        return columnHandles[static_cast<size_t>(type)];
    }
    
    const std::unique_ptr<rocksdb::ColumnFamilyHandle> &AddressIndex::getNestedColumn(AddressType::Enum type) const {
        return columnHandles[AddressType::size + static_cast<size_t>(type)];
    }
    
    ranges::any_view<InoutPointer, ranges::category::forward> AddressIndex::getOutputPointers(ChainId::Enum chainId, const RawAddress &address) const {
        auto prefixDataScriptNum = reinterpret_cast<const char *>(&address.scriptNum);
        auto prefixDataChainId = reinterpret_cast<const char *>(&chainId);
        std::vector<char> prefix(prefixDataScriptNum, prefixDataScriptNum + sizeof(address.scriptNum));  // vector with scriptNum bytes
        prefix.insert(prefix.end(), prefixDataChainId, prefixDataChainId + sizeof(chainId));   // add chainId bytes

        auto rawOutputPointerRange = ColumnIterator(db.get(), getOutputColumn(address.type).get(), prefix);
        return rawOutputPointerRange | ranges::view::transform([](std::pair<MemoryView, MemoryView> pair) -> InoutPointer {
            InoutPointer outPoint;

            uint8_t txNumData[4];
            uint8_t outputNumData[2];

            auto &key = pair.first;  // Query result
            key.data += sizeof(uint32_t);  // Skip the scriptNum in the key (first 4 bytes), as it is known already

            memcpy(&outPoint.chainId, key.data, sizeof(ChainId::Enum));
            key.data += sizeof(ChainId::Enum); // Move the key.data pointer forward

            memcpy(txNumData, key.data, 4);
            key.data += sizeof(txNumData); // Move the key.data pointer forward

            memcpy(outputNumData, key.data, 2);

            // chainId is set above, endian::big_endian::get() is not required for chainId as it's just one byte
            endian::big_endian::get(outPoint.txNum, txNumData);
            endian::big_endian::get(outPoint.inoutNum, outputNumData);
            return outPoint;
        });
    }
    
    ranges::any_view<RawAddress> AddressIndex::getIncludingMultisigs(const RawAddress &searchAddress) const {
        if (dedupType(searchAddress.type) != DedupAddressType::PUBKEY) {
            return {};
        }
        
        auto prefixData = reinterpret_cast<const char *>(&searchAddress.scriptNum);
        std::vector<char> prefix(prefixData, prefixData + sizeof(searchAddress.scriptNum));
        auto rawDedupAddressRange = ColumnIterator(db.get(), getNestedColumn(AddressType::MULTISIG_PUBKEY).get(), prefix);
        return rawDedupAddressRange | ranges::view::transform([](std::pair<MemoryView, MemoryView> pair) -> RawAddress {
            auto &key = pair.first;
            key.data += sizeof(uint32_t);
            DedupAddress rawParent;
            memcpy(&rawParent, key.data, sizeof(rawParent));
            return RawAddress{rawParent.scriptNum, AddressType::MULTISIG};
        });
    }
    
    void AddressIndex::addNestedAddresses(std::vector<std::pair<blocksci::RawAddress, blocksci::DedupAddress>> nestedCache) {
        rocksdb::WriteBatch batch;
        for (auto &pair : nestedCache) {
            const RawAddress &childAddress = pair.first;
            const DedupAddress &parentAddress = pair.second;
            std::array<rocksdb::Slice, 2> keyParts = {{
                rocksdb::Slice(reinterpret_cast<const char *>(&childAddress.scriptNum), sizeof(childAddress.scriptNum)),
                rocksdb::Slice(reinterpret_cast<const char *>(&parentAddress), sizeof(parentAddress))
            }};
            std::string sliceStr;
            rocksdb::Slice key{rocksdb::SliceParts{keyParts.data(), keyParts.size()}, &sliceStr};
            auto &nestedColumn = getNestedColumn(childAddress.type);
            batch.Put(nestedColumn.get(), key, rocksdb::Slice{});
        }
        writeBatch(batch);
    }

    void AddressIndex::addOutputAddresses(std::vector<std::pair<RawAddress, InoutPointer>> outputCache) {
        rocksdb::WriteBatch batch;
        for (auto &pair : outputCache) {
            const RawAddress &address = pair.first;
            const InoutPointer &pointer = pair.second;
            uint8_t chainId;
            uint8_t txNumData[4];
            uint8_t outputNumData[2];
            // the endian::big_endian::put() is used to ensure ascending ordering of the keys in RocksDB (it uses BytewiseComparator to sort)
            endian::big_endian::put(pointer.chainId, &chainId);
            endian::big_endian::put(pointer.txNum, txNumData);
            endian::big_endian::put(pointer.inoutNum, outputNumData);
            std::array<rocksdb::Slice, 4> keyParts = {{
                rocksdb::Slice(reinterpret_cast<const char *>(&address.scriptNum), sizeof(address.scriptNum)),
                rocksdb::Slice(reinterpret_cast<const char *>(&chainId), sizeof(chainId)),
                rocksdb::Slice(reinterpret_cast<const char *>(&txNumData[0]), 4),
                rocksdb::Slice(reinterpret_cast<const char *>(&outputNumData[0]), 2)
            }};
            std::string sliceStr;
            rocksdb::Slice key{rocksdb::SliceParts{keyParts.data(), keyParts.size()}, &sliceStr};
            auto &outputColumn = getOutputColumn(address.type);
            batch.Put(outputColumn.get(), key, rocksdb::Slice{});
        }
        writeBatch(batch);
    }

    // todo: it seems like this method is never called and thus, the AdressIndex may/will be in a wrong state after a rollback (eg. due to a block reorg)
    void AddressIndex::rollback(ChainId::Enum chainId, uint32_t txNum) {
        for_each(AddressType::all(), [&](auto type) {
            auto &column = getOutputColumn(type);
            auto it = getOutputIterator(type);
            rocksdb::WriteBatch batch;
            for (it->SeekToFirst(); it->Valid(); it->Next()) {
                auto key = it->key();
                key.remove_prefix(sizeof(uint32_t));
                InoutPointer outPoint;
                memcpy(&outPoint, key.data(), sizeof(outPoint));
                if (outPoint.chainId == chainId && outPoint.txNum >= txNum) {
                    batch.Delete(column.get(), it->key());
                }
            }
            assert(it->status().ok()); // Check for any errors found during the scan
        });
    }
    
    ranges::optional<DedupAddress> AddressIndex::getNestingScriptHash(const RawAddress &searchAddress) const {
        rocksdb::Slice key{reinterpret_cast<const char *>(&searchAddress.scriptNum), sizeof(searchAddress.scriptNum)};
        auto it = getNestedIterator(searchAddress.type);
        it->Seek(key);
        if (it->Valid() && it->key().starts_with(key)) {
            auto foundKey = it->key();
            foundKey.remove_prefix(sizeof(uint32_t));
            DedupAddress rawParent;
            memcpy(&rawParent, foundKey.data(), sizeof(rawParent));
            return rawParent;
        }
        return ranges::nullopt;
    }
    
    std::unordered_set<DedupAddress> AddressIndex::getPossibleNestedEquivalentUp(const RawAddress &searchAddress) const {
        std::unordered_set<RawAddress> addressesToSearch{searchAddress};
        std::unordered_set<DedupAddress> searchedAddresses;
        while (addressesToSearch.size() > 0) {
            auto address = *addressesToSearch.begin();
            auto nestingAddress = getNestingScriptHash(address);
            if (nestingAddress) {
                if (nestingAddress->type == DedupAddressType::SCRIPTHASH) {
                    if (searchedAddresses.find(*nestingAddress) == searchedAddresses.end()) {
                        for (auto type : equivAddressTypes(dedupType(AddressType::SCRIPTHASH))) {
                            addressesToSearch.insert({nestingAddress->scriptNum, type});
                        }
                    }
                }
            }
            searchedAddresses.insert({address.scriptNum, dedupType(address.type)});
            addressesToSearch.erase(addressesToSearch.begin());
        }
        return searchedAddresses;
    }
}

