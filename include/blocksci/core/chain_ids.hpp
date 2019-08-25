//
//  chain_ids.hpp
//  blocksci_devel
//

#ifndef blocksci_chain_ids_hpp
#define blocksci_chain_ids_hpp

#include <blocksci/blocksci_export.h>

#include <cstddef>
#include <functional>
#include <tuple>


namespace blocksci {
    // todo-fork: add other coins and assign final IDs
    class ChainId {
    public:
        enum Enum : uint16_t
        {
            UNSPECIFIED = 0,
            BITCOIN = 1,
            BITCOIN_CASH = 2,
            BITCOIN_SV = 3,
            BITCOIN_GOLD = 4,
            LITECOIN = 100,
        };

        static std::string getName(const ChainId::Enum& chainId) {
            switch(chainId) {
                case ChainId::UNSPECIFIED: return "unspecified";
                case ChainId::BITCOIN: return "bitcoin";
                case ChainId::BITCOIN_CASH: return "bitcoin_cash";
                case ChainId::BITCOIN_SV: return "bitcoin_sv";
                case ChainId::BITCOIN_GOLD: return "bitcoin_gold";
                case ChainId::LITECOIN: return "litecoin";
            }
            return "ERROR";
            // throw error here
        }

        static ChainId::Enum get(std::string name) {
            if (name == "bitcoin") {
                return ChainId::BITCOIN;
            }
            else if (name == "bitcoin_cash") {
                return ChainId::BITCOIN_CASH;
            }
        }
    };
}

#endif /* blocksci_chain_ids_hpp */
