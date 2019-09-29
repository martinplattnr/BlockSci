//
//  main.cpp
//  blocksci-test
//
//  Created by Harry Kalodner on 1/3/17.
//  Copyright © 2017 Harry Kalodner. All rights reserved.
//

#include <blocksci/chain/blockchain.hpp>
#include <blocksci/cluster/cluster_manager.hpp>
#include <blocksci/heuristics/change_address.hpp>

#include <clipp.h>

#include <iostream>

int main(int argc, char * argv[]) {
    std::string configLocation;
    std::string outputLocation;
    bool overwrite;
    auto cli = (
                clipp::value("config file location", configLocation),
                clipp::value("output location", outputLocation),
                clipp::option("--overwrite").set(overwrite).doc("Overwrite existing cluster files if they exist")
    );
    auto res = parse(argc, argv, cli);
    if (res.any_error()) {
        std::cout << "Invalid command line parameter\n" << clipp::make_man_page(cli, argv[0]);
        return 0;
    }
    
    // todo-fork: initialized with the forked blockchain or ChainManager
    //blocksci::Blockchain chain(configLocation);

    //blocksci::Blockchain btcChain("/home/martin/testchains/blocksci/btc-btc/btc-main-config-forkenabled.json");
    //blocksci::Blockchain btcForkChain("/home/martin/testchains/blocksci/btc-btc/btc-fork-config-forkenabled.json");

    blocksci::Blockchain btcChain("/mnt/data/blocksci/bitcoin/current-root-v0.6/config.json");
    blocksci::Blockchain bchForkChain("/mnt/data/blocksci/bitcoin-cash/current-fork-v0.6/config.json");

    std::vector<blocksci::BlockRange> chains;
    chains.push_back(btcChain);
    chains.push_back(bchForkChain);

    blocksci::ClusterManager::createClustering(chains, blocksci::heuristics::NoChange{}, outputLocation, overwrite);
    return 0;
}
