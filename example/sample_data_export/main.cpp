#include <iostream>
#include <blocksci/blocksci.hpp>
#include <internal/progress_bar.hpp>
#include <internal/address_info.hpp>

#include <clipp.h>
#include <fstream>

int main(int argc, char * argv[]) {
    std::string configLocation;
    std::string outputDirectory;

    int32_t includeEveryNthBlock = 2000;

    bool exportAddresses = false;
    int32_t includeEveryNthTxAddresses = 750;

    int32_t blockHeight = 0;

    int32_t threads = std::thread::hardware_concurrency();

    auto cli = (
        clipp::value("config file location", configLocation),
        clipp::value("output location", outputDirectory),
        clipp::option("-b", "--n-th-blocks").doc("Export blocks, txes, inputs, and outputs for every n-th block") & clipp::value("every-n-th-block=2000", includeEveryNthBlock),
        clipp::option("-a", "--n-th-tx-addresses").set(exportAddresses).doc("Export addresses of every n-th transaction within the selected blocks") & clipp::opt_value("n-th-tx-addresses=750", includeEveryNthTxAddresses),
        clipp::option("--block-height").doc("Export blocks upto the given height. This also affects address balance calculation.") & clipp::value("block-height=0", blockHeight),
        clipp::option("-t", "--threads").doc("Number of threads to use. Separate output files are created by every thread.") & clipp::value("threads=available_hardware_threads", threads)
    );
    auto res = parse(argc, argv, cli);
    if (res.any_error()) {
        std::cout << "Invalid command line parameter\n" << clipp::make_man_page(cli, argv[0]);
        return 0;
    }

    blocksci::Blockchain chain{configLocation, blockHeight};
    //blocksci::Blockchain chainFull{configLocation};
    std::cout << "Exporting every " << includeEveryNthBlock << "-th block with transactions, inputs, outputs";
    if (exportAddresses) {
        std::cout << "; and addresses of every " << includeEveryNthTxAddresses << "-th transaction";
    }
    std::cout << std::endl << "Using data upto block height " << blockHeight << std::endl;

    // extract lambda that is used with BlockRange.mapReduce()
    auto extract = [&](const blocksci::BlockRange &blocks, int threadNum) {
        std::ofstream blockFile, txFile, inputFile, outputFile, addressFile;

        blockFile.open (outputDirectory + "/blocks" + std::to_string(threadNum) + ".txt");
        txFile.open (outputDirectory + "/transactions" + std::to_string(threadNum) + ".txt");
        inputFile.open (outputDirectory + "/inputs" + std::to_string(threadNum) + ".txt");
        outputFile.open (outputDirectory + "/outputs" + std::to_string(threadNum) + ".txt");
        if (exportAddresses) {
            addressFile.open (outputDirectory + "/addresses" + std::to_string(threadNum) + ".txt");
        }

        auto progressThread = static_cast<int>(std::thread::hardware_concurrency()) - 1;
        auto progressBar = blocksci::makeProgressBar(blocks.endTxIndex() - blocks.firstTxIndex(), [=]() {});
        if (threadNum != progressThread) {
            progressBar.setSilent();
        }
        uint32_t txNum = 0;
        for (auto block : blocks) {
            if (block.height() % includeEveryNthBlock != 0) {
                txNum += block.size();
                continue;
            }

            blockFile << block.toString() << std::endl;
            for (auto tx : block) {
                txFile << tx.toString() << std::endl;
                for (auto input : tx.inputs()) {
                    inputFile << input.toString() << std::endl;
                    if (exportAddresses && txNum % includeEveryNthTxAddresses == 0) {
                        auto address = input.getAddress();
                        //auto addressFullChain = blocksci::Address{address.scriptNum, address.type, chainFull.getAccess()};
                        auto scriptBase = address.getBaseScript();
                        addressFile << "tx " << tx.txNum << "/input " << input.pointer.inoutNum << ": " << scriptBase.toString()
                                    // calculating balance is disabled as it takes a long time
                                    //<< ": balance(" << address.calculateBalance(blockHeight) << "), "
                                    << "txFirstSeen(" << scriptBase.getFirstTxIndex().value_or(0) <<"), "
                                    << "txFirstSpent(" << scriptBase.getTxRevealedIndex().value_or(0) << ")" << std::endl;
                    }
                }
                for (auto output : tx.outputs()) {
                    outputFile << output.toString() << std::endl;
                    if (exportAddresses && txNum % includeEveryNthTxAddresses == 0) {
                        auto address = output.getAddress();
                        //auto addressFullChain = blocksci::Address{address.scriptNum, address.type, chainFull.getAccess()};
                        auto scriptBase = address.getBaseScript();
                        addressFile << "tx " << tx.txNum << "/output " << output.pointer.inoutNum << ": " << scriptBase.toString()
                                    // calculating balance is disabled as it takes a long time
                                    //<< ": balance(" << address.calculateBalance(blockHeight) << "), "
                                    << "txFirstSeen(" << scriptBase.getFirstTxIndex().value_or(0) <<"), "
                                    << "txFirstSpent(" << scriptBase.getTxRevealedIndex().value_or(0) << ")" << std::endl;
                    }
                }
                if (threadNum == progressThread) {
                    progressBar.update(txNum);
                }
                txNum++;
            }
        }
        return 0;
    };

    chain.mapReduce<int>(extract, [](int &a,int &) -> int & {return a;}, threads);

    return 0;
}
