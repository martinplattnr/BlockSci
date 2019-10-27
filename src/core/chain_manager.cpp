//
// Created by martin on 23.08.19.
//


#include <blocksci/core/chain_manager.hpp>

#include <internal/data_access.hpp>
#include <internal/data_configuration.hpp>

#include <iostream>

namespace blocksci {
    void ChainManager::loadFullChain(const std::string &configPath) {
        std::cout << "loading " << configPath << std::endl;
    }
}
