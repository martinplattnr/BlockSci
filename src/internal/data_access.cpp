//
//  data_access.cpp
//  blocksci
//
//  Created by Harry Kalodner on 4/24/18.
//
//

#include "data_access.hpp"
#include "chain_access.hpp"
#include "script_access.hpp"
#include "address_index.hpp"
#include "hash_index.hpp"
#include "mempool_index.hpp"

namespace blocksci {
    
    DataAccess::DataAccess() = default;

    DataAccess::DataAccess(DataConfiguration config_) :
    config(std::move(config_)),
    //chain{std::make_unique<ChainAccess>(config.chainDirectory(), config.blocksIgnored, config.errorOnReorg, config.chainConfig.parentChainConfigPath, config.chainConfig.firstForkedBlockHeight)},
    chain{std::make_unique<ChainAccess>(config)},

    scripts{std::make_unique<ScriptAccess>(config.rootDataConfiguration().scriptsDirectory())},
    //scripts{std::make_unique<ScriptAccess>(config.scriptsDirectory())},
    addressIndex{std::make_unique<AddressIndex>(config.addressDBFilePath(), true)},
    hashIndex{std::make_unique<HashIndex>(config.hashIndexFilePath(), true)},
    mempoolIndex{std::make_unique<MempoolIndex>(config.mempoolDirectory())} {
        //ChainConfiguration* current;
        //current = &config.chainConfig;

        //ChainAccess ca{config.chainDirectory(), config.blocksIgnored, config.errorOnReorg, config.chainConfig.firstForkedBlockHeight};

        //while (current->parentChainConfiguration) {
            //current = current->parentChainConfiguration;
        //}
    }
    
    DataAccess::DataAccess(DataAccess &&) = default;
    DataAccess &DataAccess::operator=(DataAccess &&) = default;
    DataAccess::~DataAccess() = default;

    void DataAccess::reload() {
        chain->reload();
        scripts->reload();
        mempoolIndex->reload();
    }
}
