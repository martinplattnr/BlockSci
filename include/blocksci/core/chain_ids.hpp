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
#include <stdexcept>


namespace blocksci {
    // todo: class could be refactored to compile-time lookup (enum -> string), see dedup_address_info.cpp
    class ChainId {
    public:
        enum Enum : uint8_t // if the type is changed, code must be adapted, eg. AddressIndex::getOutputPointers()
        {
            UNSPECIFIED = 0,

            BITCOIN = 10,
            BITCOIN_TESTNET,
            BITCOIN_REGTEST,

            BITCOIN_CASH=20,
            BITCOIN_CASH_TESTNET,
            BITCOIN_CASH_REGTEST,

            BITCOIN_CASH_SV=30,
            BITCOIN_CASH_SV_TESTNET,
            BITCOIN_CASH_SV_REGTEST,

            LITECOIN=100,
            LITECOIN_TESTNET,
            LITECOIN_REGTEST,

            DASH=105,
            DASH_TESTNET,

            NAMECOIN=110,
            NAMECOIN_TESTNET,

            ZCASH=115,
            ZCASH_TESTNET
        };

        static inline std::string getName(const ChainId::Enum& chainId) {
            switch(chainId) {
                case ChainId::UNSPECIFIED : return "unspecified";

                case ChainId::BITCOIN: return "bitcoin";
                case ChainId::BITCOIN_TESTNET: return "bitcoin_testnet";
                case ChainId::BITCOIN_REGTEST: return "bitcoin_regtest";

                case ChainId::BITCOIN_CASH: return "bitcoin_cash";
                case ChainId::BITCOIN_CASH_TESTNET: return "bitcoin_cash_testnet";
                case ChainId::BITCOIN_CASH_REGTEST: return "bitcoin_cash_regtest";

                case ChainId::BITCOIN_CASH_SV: return "bitcoin_cash_sv";
                case ChainId::BITCOIN_CASH_SV_TESTNET: return "bitcoin_cash_sv_testnet";
                case ChainId::BITCOIN_CASH_SV_REGTEST: return "bitcoin_cash_sv_regtest";

                case ChainId::LITECOIN: return "litecoin";
                case ChainId::LITECOIN_TESTNET: return "litecoin_testnet";
                case ChainId::LITECOIN_REGTEST: return "litecoin_regtest";

                case ChainId::DASH: return "dash";
                case ChainId::DASH_TESTNET: return "dash_testnet";

                case ChainId::NAMECOIN: return "namecoin";
                case ChainId::NAMECOIN_TESTNET: return "namecoin_testnet";

                case ChainId::ZCASH: return "zcash";
                case ChainId::ZCASH_TESTNET: return "zcash_testnet";
            }

            throw std::runtime_error("unknown chain id");
        }

        static ChainId::Enum get(const std::string& name) {
            if (name == "unspecified") {
                return ChainId::UNSPECIFIED;
            }

            else if (name == "bitcoin") {
                return ChainId::BITCOIN;
            }
            else if (name == "bitcoin_testnet") {
                return ChainId::BITCOIN_TESTNET;
            }
            else if (name == "bitcoin_regtest") {
                return ChainId::BITCOIN_REGTEST;
            }

            else if (name == "bitcoin_cash") {
                return ChainId::BITCOIN_CASH;
            }
            else if (name == "bitcoin_cash_testnet") {
                return ChainId::BITCOIN_CASH_TESTNET;
            }
            else if (name == "bitcoin_cash_regtest") {
                return ChainId::BITCOIN_CASH_REGTEST;
            }

            else if (name == "bitcoin_cash_sv") {
                return ChainId::BITCOIN_CASH_SV;
            }
            else if (name == "bitcoin_cash_sv_testnet") {
                return ChainId::BITCOIN_CASH_SV_TESTNET;
            }
            else if (name == "bitcoin_cash_sv_regtest") {
                return ChainId::BITCOIN_CASH_SV_REGTEST;
            }

            else if (name == "litecoin") {
                return ChainId::LITECOIN;
            }
            else if (name == "litecoin_testnet") {
                return ChainId::LITECOIN_TESTNET;
            }
            else if (name == "litecoin_regtest") {
                return ChainId::LITECOIN_REGTEST;
            }

            else if (name == "dash") {
                return ChainId::DASH;
            }
            else if (name == "dash_testnet") {
                return ChainId::DASH_TESTNET;
            }

            else if (name == "namecoin") {
                return ChainId::NAMECOIN;
            }
            else if (name == "namecoin_testnet") {
                return ChainId::NAMECOIN_TESTNET;
            }

            else if (name == "zcash") {
                return ChainId::ZCASH;
            }
            else if (name == "zcash_testnet") {
                return ChainId::ZCASH_TESTNET;
            }

            throw std::runtime_error("unknown chain name");
        }
    };
}

#endif /* blocksci_chain_ids_hpp */
