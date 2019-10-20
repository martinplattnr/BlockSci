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
    std::vector<std::string> configLocations;
    std::string outputLocation;
    std::string reduceToChain;
    bool overwrite;
    auto cli = (
        clipp::values("chain config file(s)", configLocations).doc("Chain config file(s); if more than one, all chains must have been parsed in a multi-chain configuration").required(true),
        (clipp::required("-o", "--output-directory") & clipp::value("output location", outputLocation)).doc("Directory where clustering output is saved"),
        clipp::option("-r", "--reduce-to") & clipp::value("coin-name", reduceToChain),
        clipp::option("--overwrite").set(overwrite).doc("Overwrite existing cluster files if they exist")
    );
    auto res = parse(argc, argv, cli);
    if (res.any_error()) {
        std::cout << "Invalid command line parameter\n" << clipp::make_man_page(cli, argv[0]);
        return 0;
    }

    std::vector<blocksci::BlockRange*> chainsToCluster;
    for (const auto& configLocation : configLocations) {
        chainsToCluster.push_back(new blocksci::Blockchain{configLocation});
    }

    blocksci::ChainId::Enum reduceToChainId = blocksci::ChainId::UNSPECIFIED;
    if ( ! reduceToChain.empty()) {
        reduceToChainId = blocksci::ChainId::get(reduceToChain);
    }

    blocksci::ClusterManager::createClustering(chainsToCluster, blocksci::heuristics::NoChange{}, outputLocation, overwrite, reduceToChainId);
    return 0;
}
