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
    class BLOCKSCI_EXPORT ChainId {
    public:
        static const uint8_t RESERVED = 0;

        static const uint8_t BITCOIN = 1;
        static const uint8_t BITCOIN_TESTNET = 2;
        static const uint8_t BITCOIN_REGTEST = 3;

        static const uint8_t BITCOIN_CASH = 4;
        static const uint8_t BITCOIN_CASH_TESTNET = 5;
        static const uint8_t BITCOIN_CASH_REGTEST = 6;

        static const uint8_t LITECOIN = 7;
        static const uint8_t LITECOIN_TESTNET = 8;
        static const uint8_t LITECOIN_REGTEST = 9;

        static const uint8_t DASH = 10;
        static const uint8_t DASH_TESTNET = 11;

        static const uint8_t ZCASH = 12;
        static const uint8_t ZCASH_TESTNET = 13;

        static const uint8_t NAMECOIN = 14;
        static const uint8_t NAMECOIN_TESTNET = 15;

    };
}

#endif /* blocksci_chain_ids_hpp */
