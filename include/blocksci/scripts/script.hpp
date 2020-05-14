//
//  script.hpp
//  blocksci_devel
//
//  Created by Harry Kalodner on 3/6/17.
//  Copyright © 2017 Harry Kalodner. All rights reserved.
//

#ifndef script_hpp
#define script_hpp

#include <blocksci/core/script_data.hpp>
#include <blocksci/address/address.hpp>

namespace blocksci {
    struct DataConfiguration;
    class DataAccess;
    
    class BLOCKSCI_EXPORT ScriptBase : public Address  {
        const ScriptHeader *rawHeader;
        const ScriptDataBase *rawData;

    protected:
        const ScriptDataBase *getData() const {
            return rawData;
        }
    public:
        ScriptBase() = default;
        ScriptBase(uint32_t scriptNum_, AddressType::Enum type_, DataAccess &access_, const ScriptHeader *scriptHeader_, const ScriptDataBase *scriptData_)
            : Address(scriptNum_, type_, access_), rawHeader(scriptHeader_), rawData(scriptData_) {}
        
        explicit ScriptBase(const Address &address);
        
        void visitPointers(const std::function<void(const Address &)> &) const {}

        /** Checks if a type equivalent address has ever been seen (only relevant in multi-chain mode, otherwise always True) */
        bool hasBeenSeen() const {
            return rawHeader->txFirstSeen != std::numeric_limits<uint32_t>::max();
        }

        ranges::optional<uint32_t> getFirstTxIndex() const {
            auto txSeenIndex = rawHeader->txFirstSeen;
            if (txSeenIndex != std::numeric_limits<uint32_t>::max()) {
                return txSeenIndex;
            }
            else {
                return ranges::nullopt;
            }
        }
        
        ranges::optional<uint32_t> getTxRevealedIndex() const {
            auto txRevealedIndex = rawHeader->txFirstSpent;
            if (txRevealedIndex != std::numeric_limits<uint32_t>::max()) {
                return txRevealedIndex;
            } else {
                return ranges::nullopt;
            }
        }

        bool hasBeenSpent() const {
            return getTxRevealedIndex().has_value();
        }

        uint32_t getTypesSeen() const {
            return rawHeader->typesSeen;
        }

        ranges::optional<Transaction> getFirstTransaction() const;
        ranges::optional<Transaction> getTransactionRevealed() const;
    };
} // namespace blocksci

namespace std {
    template <>
    struct hash<blocksci::ScriptBase> {
        typedef blocksci::ScriptBase argument_type;
        typedef size_t  result_type;
        result_type operator()(const blocksci::ScriptBase &b) const {
            return std::hash<blocksci::Address>{}(b);
        }
    };
}

#endif /* script_hpp */
