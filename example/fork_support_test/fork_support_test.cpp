#include <iostream>
#include <blocksci/blocksci.hpp>

int main(int argc, const char * argv[]) {
    blocksci::Blockchain btcForkStandalone{"/home/martin/testchains/blocksci/btc-btc/btc-fork-standalone-config.json"};
    blocksci::Blockchain btcForkShared{"/home/martin/testchains/blocksci/btc-btc/btc-fork-config-forkenabled.json"};

    auto btcForkStandaloneIt = btcForkStandalone.begin();
    auto btcForkSharedIt = btcForkShared.begin();

    // loop blocks
    while (btcForkStandaloneIt != btcForkStandalone.end() || btcForkSharedIt != btcForkShared.end()) {
        std::cout << "### GET BLOCK" << std::endl;
        auto saBlock = *btcForkStandaloneIt;
        auto shBlock = *btcForkSharedIt;

        if (saBlock != shBlock
        && saBlock.getHash() == shBlock.getHash()
        && saBlock.height() == shBlock.height()
        && saBlock.version() == shBlock.version()
        && saBlock.timestamp() == saBlock.timestamp()
        && saBlock.bits() == saBlock.bits()
        && saBlock.nonce() == saBlock.nonce()
        && saBlock.baseSize() == saBlock.baseSize()
        && saBlock.totalSize() == saBlock.totalSize()
        && saBlock.virtualSize() == saBlock.virtualSize()
        && saBlock.baseSize() == saBlock.baseSize()
        && saBlock.weight() == saBlock.weight()
        && saBlock.sizeBytes() == saBlock.sizeBytes()
        && saBlock.baseSize() == saBlock.baseSize()
        && saBlock.getHeaderHash() == saBlock.getHeaderHash()
        && saBlock.getCoinbase() == saBlock.getCoinbase()
        && saBlock.coinbaseTx() == saBlock.coinbaseTx()) {
            std::cout << "Block: DIFFERENCE" << std::endl;
        }
        else {
            std::cout << "Block: OK" << std::endl;
        }

        auto saTxIt = saBlock.begin();
        auto saTxItEnd = saBlock.end();

        auto shTxIt = shBlock.begin();
        auto shTxItEnd = shBlock.end();

        // loop transactions
        while (saTxIt != saTxItEnd || shTxIt != shTxItEnd) {
            std::cout << "### GET TRANSACTION" << std::endl;
            auto saTx = *saTxIt;
            auto shTx = *shTxIt;


            if (saTx != shTx
            && saTx.getHash() == shTx.getHash()
            && saTx.getVersion() == shTx.getVersion()
            && saTx.calculateBlockHeight() == shTx.calculateBlockHeight()
            && saTx.getBlockHeight() == shTx.getBlockHeight()
            && saTx.fee() == shTx.fee()
            && saTx.isCoinbase() == shTx.isCoinbase()) {
                std::cout << "Transaction: DIFFERENCE" << std::endl;
            }
            else {
                std::cout << "Transaction: OK" << std::endl;
            }

            auto saInputs = saTx.inputs();
            auto shInputs = shTx.inputs();

            auto saInputIt = saInputs.begin();
            auto shInputIt = shInputs.begin();

            while (saInputIt != saInputs.end() || shInputIt != shInputs.end()) {
                std::cout << "### GET INPUT" << std::endl;
                auto saInput = *saInputIt;
                auto shInput = *shInputIt;

                if (saInput != shInput) {
                    std::cout << "Input: DIFFERENCE" << std::endl;
                }
                else {
                    std::cout << "Input: OK" << std::endl;
                }

                if(saInputIt != saInputs.end())
                {
                    ++saInputIt;
                }
                if(shInputIt != shInputs.end())
                {
                    ++shInputIt;
                }
            }

            auto saOutputs = saTx.outputs();
            auto shOutputs = shTx.outputs();

            auto saOutputIt = saOutputs.begin();
            auto shOutputIt = shOutputs.begin();

            while (saOutputIt != saOutputs.end() || shOutputIt != shOutputs.end()) {
                std::cout << "### GET OUTPUT" << std::endl;
                auto saOutput = *saOutputIt;
                auto shOutput = *shOutputIt;

                if (saOutput != shOutput) {
                    std::cout << "Output: DIFFERENCE" << std::endl;
                }
                else {
                    std::cout << "Output: OK" << std::endl;
                }

                if(saOutputIt != saOutputs.end())
                {
                    ++saOutputIt;
                }
                if(shOutputIt != shOutputs.end())
                {
                    ++shOutputIt;
                }
            }

            if(saTxIt != saTxItEnd)
            {
                ++saTxIt;
            }
            if(shTxIt != shTxItEnd)
            {
                ++shTxIt;
            }
        }

        if(btcForkStandaloneIt != btcForkStandalone.end())
        {
            ++btcForkStandaloneIt;
        }
        if(btcForkSharedIt != btcForkShared.end())
        {
            ++btcForkSharedIt;
        }
    }

    return 0;
}
