#include <iostream>
#include <blocksci/blocksci.hpp>

int main(int argc, const char * argv[]) {
    std::cout << "### Loading BTC ###" << std::endl;
    blocksci::Blockchain btc{"/mnt/data/blocksci/bitcoin/current-v0.6/config.json"};

    std::cout << "### Loading BCH ###" << std::endl;
    blocksci::Blockchain bch{"/mnt/data/blocksci/bitcoin-cash/current/config_fork_support.json"};

    /*
    RANGES_FOR(auto block, chain) {
        if (block.size() > 1) {
            std::cout << "Block " << block.height() << ": " << block.size() << " transactions" << std::endl;
        }
        if (block.height() > 5000) {
            break;
        }

        RANGES_FOR(auto tx, block) {
            std::cout << "tx: " << tx.txNum << "(size " << tx.totalSize() << ")" << std::endl;
        }

    }
    */

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

    /*
    for (int i=450000; i < 451000; i++) {
        auto block = btc[i];
        RANGES_FOR(auto tx, block) {
            auto txNum = tx.txNum;
        }
    }
    */

    std::getchar();

    return 0;
}
