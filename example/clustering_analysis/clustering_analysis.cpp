//
// Created by martin on 13.11.19.
//


#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>

#include <iostream>

int main(int argc, char * argv[]) {
    std::cout << "Warning: This program is incomplete and not ready for usage." << std::endl;

    std::vector<blocksci::ClusterManager> clusteringSnapshots;

    blocksci::Blockchain btc {"path"};
    blocksci::Blockchain bch {"path"};

    clusteringSnapshots.emplace_back(blocksci::ClusterManager("asd", bch.getAccess()));

    std::map<uint32_t, uint32_t> snapshotTimestamps = {
        {478559, 0},
        {500000, 0}
    };
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t, blocksci::AddressType::Enum, uint32_t, blocksci::AddressType::Enum, uint32_t>> csvInput;

    // input:  (blk, blk.timestamp, txnum, a1.type, a1.num, a2.type, a2.num)
    // output: (blk, blk.timestamp, txnum, a1.type, a1.num, a2.type, a2.num, timedelta)

    uint32_t blockHeight, blockTimestamp, txNum, addr1Num, addr2Num;
    blocksci::AddressType::Enum addr1Type, addr2Type;

    // file pointer
    std::fstream fout;

    // opens an existing csv file or creates a new file.
    fout.open("ccc_output.csv", std::ios::out | std::ios::app);

    for (auto line : csvInput) {
        std::tie(blockHeight, blockTimestamp, txNum, addr1Num, addr1Type, addr2Num, addr2Type) = line;

        // create addresses
        auto addr1 = blocksci::Address{addr1Num, addr1Type, btc.getAccess()};
        auto addr2 = blocksci::Address{addr2Num, addr2Type, btc.getAccess()};
        auto scriptBase1 = addr1.getBaseScript();
        auto scriptBase2 = addr2.getBaseScript();

        uint32_t alreadyClusteredInSnapshot = -1;
        for (std::size_t i = 0; i != clusteringSnapshots.size(); ++i) {
            // get clusters
            auto cl1 = clusteringSnapshots[i].getCluster(addr1);
            auto cl2 = clusteringSnapshots[i].getCluster(addr2);

            if (cl1 && cl2 && cl1 == cl2) {
                // addresses are in the same cluster
                alreadyClusteredInSnapshot = i;

                uint32_t timeDelta = blockTimestamp - snapshotTimestamps[i];

                fout << blockHeight << ","
                     << blockTimestamp << ","
                     << txNum << ","
                     << addr1Num << ","
                     << addr1Type << ","
                     << addr2Num << ","
                     << addr2Type << ","
                     << timeDelta
                     << "\n";
                break;
            }
        }
        
        if (alreadyClusteredInSnapshot == -1) {
            // addresses have not been clustered
            fout << blockHeight << ","
                 << blockTimestamp << ","
                 << txNum << ","
                 << addr1Num << ","
                 << addr1Type << ","
                 << addr2Num << ","
                 << addr2Type << ","
                 << "\n";
        }
    }

    return 0;
}
