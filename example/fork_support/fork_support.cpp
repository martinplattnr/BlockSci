#include <iostream>
#include <blocksci/blocksci.hpp>

int main(int argc, const char * argv[]) {
    blocksci::Blockchain chain{"/storage/ifishare/blksci/blocksci/bitcoin/current-v0.6/config.json"};

    RANGES_FOR(auto block, chain) {
        if (block.size() > 1) {
            std::cout << "Block " << block.height() << ": " << block.size() << " transactions" << std::endl;
        }
        if (block.height() > 5000) {
            break;
        }
    }

    return 0;
}
