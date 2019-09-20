//
// Created by martin on 23.08.19.
//

#ifndef BLOCKSCI_CHAIN_MANAGER_HPP
#define BLOCKSCI_CHAIN_MANAGER_HPP

#endif //BLOCKSCI_CHAIN_MANAGER_HPP

#include <blocksci/blocksci_export.h>
#include <blocksci/core/chain_ids.hpp>

#include <map>
#include "typedefs.hpp"

namespace blocksci {
    class DataAccess;

    class BLOCKSCI_EXPORT ChainManager {
        std::map<ChainId::Enum, DataAccess> dataAccessMap;

    public:
        void loadFullChain(const std::string &configPath);
        void loadChain(const std::string &configPath, bool errorOnReorg, BlockHeight blocksIgnored);
    };
}
