#include <iostream>
#include <blocksci/blocksci.hpp>
#include <blocksci/core/chain_manager.hpp>

int main(int argc, const char * argv[]) {

    blocksci::ChainManager asd;
    asd.loadFullChain("/home/martin/testchains/blocksci/btc-btc/btc-fork-config.json");

    return 0;

    std::cout << "### Loading BTC fork ###" << std::endl;
    //blocksci::Blockchain btc{"/mnt/data/blocksci/bitcoin/current-v0.6/config.json"};
    blocksci::Blockchain btcFork{"/home/martin/testchains/blocksci/btc-btc/btc-fork-config.json"};

    std::cout << "### Loading BTC main ###" << std::endl;
    blocksci::Blockchain btcMain{"/home/martin/testchains/blocksci/btc-btc/btc-main-config.json"};

    //blocksci::Blockchain bch{"/mnt/data/blocksci/bitcoin-cash/current/config_fork_support.json"};

    /*
    for(auto block: btcFork) {
        for(auto tx: block) {
            std::cout << "tx: " << tx.txNum << "(size " << tx.totalSize() << ")" << std::endl;
        }
    }
    return 0;

    for (int i=0; i < 15; i++) {
        auto btcForkBlock = btcFork[i];
        auto btcMainBlock = btcMain[i];

        std::cout << "### Block " << i << " ### " << std::endl;
        std::cout << "btcMain.hash = " << btcMainBlock.getHeaderHash() << std::endl;
        std::cout << "btcFork.hash = " << btcForkBlock.getHeaderHash() << std::endl;




        for(int j=0; j < btcForkBlock.size(); j++) {
            auto btcForkTx = btcForkBlock[j];
            auto btcMainTx = btcMainBlock[j];

            std::cout << "tx: " << btcForkTx.txNum << std::endl;

            auto btcForkOutputRange = btcForkTx.outputs();
            auto btcMainOutputRange = btcMainTx.outputs();

            for (int k=0; k < btcForkTx.outputCount(); k++) {
                auto btcForkOutput = btcForkOutputRange[k];
                auto btcMainOutput = btcMainOutputRange[k];

                std::cout << "output " << btcForkOutput.pointer.inoutNum << ": spendingTx = " << btcMainOutput.getSpendingTxIndex().value_or(0)
                    << " | " << btcForkOutput.getSpendingTxIndex().value_or(0) << std::endl;
            }

            //std::cout << "btcForkTx.spendingTx = " << btcForkTx
        }
    }

    return 0;
    */

    RANGES_FOR(auto block, btcMain) {
        //if (block.size() > 1) {
            std::cout << "### Block " << block.height() << ": " << block.size() << " transactions" << std::endl;
        //}
        if (block.height() > 5000) {
            break;
        }

        RANGES_FOR(auto tx, block) {
            std::cout << "tx: " << tx.txNum << "(size " << tx.totalSize() << ")" << std::endl;

            RANGES_FOR(auto output, tx.outputs()) {
                std::cout << "output: " << output.getValue() << " | spendingTx: " << output.getSpendingTxIndex().value_or(0) << std::endl;
            }
        }
    }
    afterLoop:
    /*
    double txSizes = 0.0;
    int txCount = 0;
    for (int i=478559; i < 478560; i++) {
        std::cout << "### Block " << i << " ###" << std::endl;

        auto block = bch[i];
        auto firstTxOfBlock = block[0];
        //std::cout << "Block size: " << block.totalSize() << " bytes"
                  //<< " " << block.totalSize() / 1024 << " KB)" << std::endl;
        //std::cout << "First txnum of block: " << firstTxOfBlock.txNum << std::endl;
        //std::cout << std::endl;

        //std::cout << "addresses: btc:" << btc[i].rawBlock << " | bch: " << bch[i].rawBlock << " " << (btc[i].rawBlock == bch[i].rawBlock ? "SAME" : "DIFFERENT") << std::endl;

        RANGES_FOR(auto tx, block) {
            std::cout << "txnum: " << tx.txNum << std::endl;
            auto txSize = tx.baseSize();
            txSizes += txSize;
            txCount++;
        }
    }

    std::cout << txSizes << std::endl;
    std::cout << "txCount=" << txCount << std::endl;
    */
    /*
    for (int i=450000; i < 451000; i++) {
        auto block = btc[i];
        RANGES_FOR(auto tx, block) {
            auto txNum = tx.txNum;
        }
    }
    */

    //std::getchar();

    return 0;
}
