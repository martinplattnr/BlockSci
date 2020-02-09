//
// Created by martin on 09.02.20.
//

#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>
#include <internal/chain_access.hpp>
#include <internal/hash_index.hpp>
#include <internal/address_info.hpp>

#include <iostream>
#include <chrono>

#include <mpark/variant.hpp>


int main(int argc, char * argv[]) {
    blocksci::Blockchain btc {"/mnt/data/blocksci/btc/periods/2019-12-31/config.json"};
    auto scClustering = blocksci::ClusterManager("/mnt/data/blocksci/clusterings/btc/2019-12-31", btc.getAccess());

    std::vector<uint32_t> scClusterNums = {
        86251284, 12999543, 133099832, 341202647, 67176172, 21715802, 309851410, 3015834, 22252561,
        9594146, 12561823, 191121161, 16038639, 10198139, 16958587, 85883381, 224409590, 7432336, 109437891,
        243516711, 89019753, 9292633, 522727277, 154824890, 143863913
    };

    for (const auto &scClusterNum : scClusterNums) {
        auto scCluster = scClustering.getCluster(scClusterNum);
        auto scClusterDedupAddrs = scCluster->getDedupAddresses();
        std::cout << "SC-Cluster " << scCluster->clusterNum << " (" << scCluster->getTypeEquivSize() << " addresses)" << std::endl;
        uint32_t index = 0;
        bool foundPubkey = false;

        for (auto scDedupAddr : scClusterDedupAddrs) {
            index++;
            if (scDedupAddr.type == blocksci::DedupAddressType::PUBKEY) {
                auto addr = blocksci::Address{scDedupAddr.scriptNum, blocksci::reprType(scDedupAddr.type), btc.getAccess()};
                std::cout << addr.toString() << std::endl;
                foundPubkey = true;
                break;
            }
        }
        std::cout << "Checked " << index << " addresses for type=pubkey" << (foundPubkey == true ? " before finding match" : ", no match found") << std::endl;

        index = 0;
        if (foundPubkey == false) {
            for (auto scDedupAddr : scClusterDedupAddrs) {
                index++;
                if (scDedupAddr.type == blocksci::DedupAddressType::SCRIPTHASH) {
                    auto addr = blocksci::Address{scDedupAddr.scriptNum, blocksci::reprType(scDedupAddr.type), btc.getAccess()};
                    std::cout << addr.toString() << std::endl;
                    break;
                }
            }
            std::cout << "Tested " << index << " scripthash addresses" << std::endl;
        }
        std::cout << std::endl;
    }

    return 0;
}
