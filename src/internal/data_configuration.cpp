//
//  data_configuration.cpp
//  blocksci
//
//  Created by Harry Kalodner on 8/1/17.
//
//

#include "data_configuration.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

#include <iostream>

#include <memory>

using json = nlohmann::json;

namespace blocksci {

    // 1. BSV
    // 2. BCH
    // 3. BTC
    DataConfiguration loadBlockchainConfig(const std::string &configPath, bool errorOnReorg, BlockHeight blocksIgnored) {
        auto jsonConf = loadConfig(configPath);
        checkVersion(jsonConf);
        
        ChainConfiguration chainConfig = jsonConf.at("chainConfig");

        // todo-fork: refactor creation of DataConfiguration, shared pointer assignment is a mess
        if (chainConfig.parentChainConfigPath.length()) {
            DataConfiguration dc{configPath, chainConfig, errorOnReorg, blocksIgnored, std::make_shared<DataConfiguration>(loadBlockchainConfig(chainConfig.parentChainConfigPath.str(), errorOnReorg, blocksIgnored))};
            dc.parentDataConfiguration->childDataConfiguration = std::make_shared<DataConfiguration>(dc);
            return dc;
        }
        else {
            return DataConfiguration{configPath, chainConfig, errorOnReorg, blocksIgnored};
        }

        /*
        while (currentChainConfig->parentChainConfigPath.length()) {
            loadBlockchainConfig(currentChainConfig->parentChainConfigPath.str(), errorOnReorg, blocksIgnored);
            //std::cout << "currentChainConfig->parentChainConfigPath: " << currentChainConfig->parentChainConfigPath.length() << std::endl;
            auto parentJsonConf = loadConfig(chainConfig.parentChainConfigPath.str());
            checkVersion(jsonConf);

            ChainConfiguration parentChainConfiguration = parentJsonConf.at("chainConfig");
            currentChainConfig->parentChainConfiguration = std::make_shared<ChainConfiguration>(parentChainConfiguration);
            currentChainConfig = &parentChainConfiguration;
        }

        return {configPath, chainConfig, errorOnReorg, blocksIgnored};
         */
    }

    /*
    DataConfiguration loadBlockchainConfig(const std::string &configPath, bool errorOnReorg, BlockHeight blocksIgnored) {
        auto jsonConf = loadConfig(configPath);
        checkVersion(jsonConf);

        ChainConfiguration chainConfig = jsonConf.at("chainConfig");
        ChainConfiguration* currentChainConfig = &chainConfig;

        while (currentChainConfig->parentChainConfigPath.length()) {
            loadBlockchainConfig(currentChainConfig->parentChainConfigPath.str(), errorOnReorg, blocksIgnored);
            //std::cout << "currentChainConfig->parentChainConfigPath: " << currentChainConfig->parentChainConfigPath.length() << std::endl;
            auto parentJsonConf = loadConfig(chainConfig.parentChainConfigPath.str());
            checkVersion(jsonConf);

            ChainConfiguration parentChainConfiguration = parentJsonConf.at("chainConfig");
            currentChainConfig->parentChainConfiguration = std::make_shared<ChainConfiguration>(parentChainConfiguration);
            currentChainConfig = &parentChainConfiguration;
        }

        return {configPath, chainConfig, errorOnReorg, blocksIgnored};
    }
     */
    
    void createDirectory(const filesystem::path &dir) {
        if(!dir.exists()){
            filesystem::create_directory(dir);
        }
    }
    
    json loadConfig(const std::string &configFilePath) {
        //std::cout << "loading config file " << configFilePath << std::endl;

        filesystem::path configFile{configFilePath};
        
        if(!configFile.exists() || !configFile.is_file()) {
            std::stringstream ss;
            ss << "Error, path " << configFile.str() << " must point to to existing blocksci config file";
            throw std::runtime_error(ss.str());
        }
        
        std::ifstream rawConf(configFile.str());
        json jsonConf;
        rawConf >> jsonConf;
        return jsonConf;
    }

    /** Checks whether the provided config file's version matches the config file version of the running program */
    void checkVersion(const json &jsonConf) {
        uint64_t versionNum = jsonConf.at("version");
        if (versionNum != dataVersion) {
            throw std::runtime_error("Error, parser data is not in the correct format. To fix you must delete the data file and rerun the parser");
        }
    }

    DataConfiguration::DataConfiguration(const std::string &configPath_, ChainConfiguration &chainConfig_, bool errorOnReorg_, BlockHeight blocksIgnored_) : configPath(configPath_), errorOnReorg(errorOnReorg_), blocksIgnored(blocksIgnored_), chainConfig(chainConfig_) {
        createDirectory(chainConfig.dataDirectory);
        createDirectory(scriptsDirectory());
        createDirectory(chainDirectory());
        createDirectory(mempoolDirectory());
    }

    DataConfiguration::DataConfiguration(const std::string &configPath_, ChainConfiguration &chainConfig_, bool errorOnReorg_, BlockHeight blocksIgnored_, std::shared_ptr<DataConfiguration> parentDataConfiguration_) :configPath(configPath_), errorOnReorg(errorOnReorg_), blocksIgnored(blocksIgnored_), chainConfig(chainConfig_), parentDataConfiguration(std::move(parentDataConfiguration_)) {
        createDirectory(chainConfig.dataDirectory);
        createDirectory(scriptsDirectory());
        createDirectory(chainDirectory());
        createDirectory(mempoolDirectory());
    }
}
