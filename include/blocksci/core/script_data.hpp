//
//  script_data.hpp
//  blocksci
//
//  Created by Harry Kalodner on 7/17/17.
//
//

#ifndef script_data_hpp
#define script_data_hpp

#include <blocksci/blocksci_export.h>
#include <blocksci/core/bitcoin_uint256.hpp>
#include <blocksci/core/raw_address.hpp>
#include <blocksci/scripts/bitcoin_pubkey.hpp>

#include <array>
#include <limits>
#include <string>
#include <vector>

namespace blocksci {
    using RawPubkey = std::array<unsigned char, 65>;
    
    template <typename T, typename Index = uint32_t>
    class BLOCKSCI_EXPORT InPlaceArray {
        Index dataSize;
        
    public:
        explicit InPlaceArray(Index size) : dataSize(size) {}
        
        Index size() const {
            return dataSize;
        }
        
        T &operator[](Index index) {
            return *(reinterpret_cast<T *>(reinterpret_cast<char *>(this) + sizeof(InPlaceArray)) + index);
        }
        
        const T &operator[](Index index) const {
            return *(reinterpret_cast<const T *>(reinterpret_cast<const char *>(this) + sizeof(InPlaceArray)) + index);
        }
        
        const T *begin() const {
            return &operator[](Index{0});
        }
        
        const T *end() const {
            return &operator[](size());
        }
        
        const T *begin() {
            return &operator[](Index{0});
        }

        const T *end() {
            return &operator[](size());
        }
        
        size_t extraSize() const {
            return sizeof(T) * size();
        }
        
    };

    struct BLOCKSCI_EXPORT ScriptHeader {
        /** Transaction number where this script has first occurred in the blockchain */
        uint32_t txFirstSeen;

        /** Transaction number where this script has been spent first; if not spent yet, defaults to std::numeric_limits<uint32_t>::max() */
        uint32_t txFirstSpent;

        uint32_t typesSeen;

        // used for scripts that have not been seen yet (eg. seen on root chain, but not on the fork)
        explicit ScriptHeader() : txFirstSeen(std::numeric_limits<uint32_t>::max()), txFirstSpent(std::numeric_limits<uint32_t>::max()), typesSeen(0) {}

        explicit ScriptHeader(uint32_t txNum) : txFirstSeen(txNum), txFirstSpent(std::numeric_limits<uint32_t>::max()), typesSeen(0) {}

        void saw(blocksci::AddressType::Enum type, bool isTopLevel) {
            typesSeen |= (1u << static_cast<uint32_t>(type) * 2);
            if (isTopLevel) {
                typesSeen |= (1u << (static_cast<uint32_t>(type) * 2 + 1));
            }
        }

        bool seenTopLevel(blocksci::AddressType::Enum type) const {
            return typesSeen & (1u << (static_cast<uint32_t>(type) * 2 + 1));
        }

        bool seen(blocksci::AddressType::Enum type) const {
            return typesSeen & (1u << static_cast<uint32_t>(type) * 2);
        }
    };

    struct BLOCKSCI_EXPORT ScriptDataBase {
        void visitPointers(const std::function<void(const RawAddress &)> &) const {}
    };
    
    struct BLOCKSCI_EXPORT PubkeyData : public ScriptDataBase {
        union {
            uint160 address;
            RawPubkey pubkey;
        };
        bool hasPubkey;
        
        PubkeyData(const RawPubkey &pubkey_) : pubkey(pubkey_), hasPubkey(true) {
            // todo: this was added to avoid non-deterministic data in the parser output, can (should?) be removed again
            pubkey.fill(0);
            auto itBegin = pubkey_.begin();
            auto itEnd = itBegin + blocksci::CPubKey::GetLen(pubkey_[0]);
            std::copy(itBegin, itEnd, pubkey.begin());
        }
        PubkeyData(const uint160 &address_) : hasPubkey(false) {
            // todo: this was set to avoid non-deterministic data in the parser output, can (should?) be removed again
            pubkey.fill(0);
            address = address_;
        }
        
        size_t size() {
            return sizeof(PubkeyData);
        }
    };
    // check the size of PubkeyData as the padding 0-initialization has to be adapted if the size changes
    static_assert (sizeof(PubkeyData) == 66, "PubkeyData does not have the expected size.");
    
    struct BLOCKSCI_EXPORT ScriptHashData : public ScriptDataBase {
        union {
            uint160 hash160; // used if a P2SH script
            uint256 hash256; // used if a P2WSH script
        };
        RawAddress wrappedAddress;
        bool isSegwit;

        ScriptHashData(uint160 hash160_, const RawAddress &wrappedAddress_) : wrappedAddress(wrappedAddress_), isSegwit(false) {
            // todo: this was added to avoid non-deterministic data in the parser output, can (should?) be removed again
            hash256.SetNull();
            hash160 = hash160_;
            // set padding (last 3 bytes) to zero
            memset(reinterpret_cast<char*>(this+1) -3, 0, 3);
        }
        
        ScriptHashData(uint256 hash256_, const RawAddress &wrappedAddress_) : hash256(hash256_), wrappedAddress(wrappedAddress_), isSegwit(true) {}
        
        size_t size() {
            return sizeof(ScriptHashData);
        }

        bool hasWrappedAddress() const {
            return wrappedAddress.scriptNum != 0;
        }
        
        void visitPointers(const std::function<void(const RawAddress &)> &visitFunc) const {
            if (hasWrappedAddress()) {
                visitFunc(wrappedAddress);
            }
        }
    };
    // check the size of ScriptHashData as the padding 0-initialization has to be adapted if the size changes
    static_assert (sizeof(ScriptHashData) == 44, "ScriptHashData does not have the expected size.");
    
    struct BLOCKSCI_EXPORT MultisigData : public ScriptDataBase {
        uint8_t m;
        uint8_t n;
        InPlaceArray<uint32_t, uint16_t> addresses;
        
        MultisigData(const MultisigData &other) = delete;
        MultisigData(MultisigData &&other) = delete;
        MultisigData &operator=(const MultisigData &other) = delete;
        MultisigData &operator=(MultisigData &&other) = delete;
                
        size_t realSize() const {
            return sizeof(MultisigData) + addresses.extraSize();
        }
        
        MultisigData(uint8_t m_, uint8_t n_, uint16_t addressCount) : m(m_), n(n_), addresses(addressCount) {}
        
        void visitPointers(const std::function<void(const RawAddress &)> &visitFunc) const {
            for (auto scriptNum : addresses) {
                visitFunc(RawAddress{scriptNum, AddressType::Enum::MULTISIG_PUBKEY});
            }
        }
    };
    
    struct BLOCKSCI_EXPORT NonstandardScriptData : public ScriptDataBase {
        InPlaceArray<unsigned char> scriptData;
        
        size_t realSize() const {
            return sizeof(NonstandardScriptData) + scriptData.extraSize();
        }
        
        NonstandardScriptData(uint32_t scriptLength) : scriptData(scriptLength) {}
    };
    
    struct BLOCKSCI_EXPORT NonstandardSpendScriptData {
        InPlaceArray<unsigned char> scriptData;
        
        size_t realSize() const {
            return sizeof(NonstandardScriptData) + scriptData.extraSize();
        }
        
        explicit NonstandardSpendScriptData(uint32_t scriptLength) : scriptData(scriptLength) {}
    };
    
    struct BLOCKSCI_EXPORT RawData : public ScriptDataBase {
        InPlaceArray<unsigned char> rawData;
        
        std::string getData() const {
            return std::string(rawData.begin(), rawData.end());
        }
        
        size_t realSize() const {
            return sizeof(RawData) + rawData.extraSize();
        }
        
        RawData(const std::vector<unsigned char> &fullData) : rawData(static_cast<uint32_t>(fullData.size())) {}
    };
    
    struct BLOCKSCI_EXPORT WitnessUnknownScriptData : public ScriptDataBase {
        uint8_t witnessVersion;
        InPlaceArray<unsigned char> scriptData;
        
        size_t realSize() const {
            return sizeof(WitnessUnknownScriptData) + scriptData.extraSize();
        }
        
        WitnessUnknownScriptData(uint8_t witnessVersion_, uint32_t scriptLength) : witnessVersion(witnessVersion_), scriptData(scriptLength) {}
    };
    
    struct BLOCKSCI_EXPORT WitnessUnknownSpendScriptData {
        InPlaceArray<unsigned char> scriptData;
        
        size_t realSize() const {
            return sizeof(WitnessUnknownSpendScriptData) + scriptData.extraSize();
        }
        
        explicit WitnessUnknownSpendScriptData(uint32_t scriptLength) : scriptData(scriptLength) {}
    };
    
} // namespace blocksci

#endif /* script_data_hpp */
