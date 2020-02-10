//
// Created by martin on 10.02.20.
//


#include <blocksci/blocksci.hpp>
#include <blocksci/cluster.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>
#include <internal/address_info.hpp>

#include <iostream>
#include <string>

using AddrUsageMap = std::unordered_map<blocksci::DedupAddress, uint32_t>;
using Chain = blocksci::Blockchain;

void handleScriptHash(blocksci::Address& addr, blocksci::BlockHeight height, AddrUsageMap& map, Chain& chain) {
    auto scriptHashData = blocksci::script::ScriptHash(addr.scriptNum, chain.getAccess());
    auto wrappedAddr = scriptHashData.getWrappedAddress();
    if (wrappedAddr) {
        addr = *wrappedAddr;
        map[{addr.scriptNum, blocksci::dedupType(addr.type)}] = height;
        if (blocksci::dedupType(addr.type) == blocksci::DedupAddressType::SCRIPTHASH) {
            handleScriptHash(addr, height, map, chain);
        }
    }
}

void handleMultisig(blocksci::Address addr, blocksci::BlockHeight height, AddrUsageMap& map, Chain& chain) {
    auto multisigData = blocksci::script::Multisig(addr.scriptNum, chain.getAccess());
    auto multisigAddrs = multisigData.getAddresses();
}

int main(int argc, char * argv[]) {
    std::vector<std::string> dates = {
            //"2017-12-31",
            //"2018-06-30",
            //"2018-12-31",
            //"2019-06-30",
            "2019-12-31"
    };

    uint32_t inactiveAfterMonths = 12;

    for (const auto &date : dates) {

        std::cout << "### PROCESSING DATE " + date + " ###" << std::endl << std::endl;

        Chain btc {"/mnt/data/blocksci/btc/periods/" + date + "/config.json"};

        /* // count multisig pubkeys
         * // result: multisigPubkeyCount=157304637
        uint32_t multisigPubkeyCount = 0;
        uint32_t multisigAddrs = btc.getAccess().scripts->scriptCount(blocksci::DedupAddressType::MULTISIG);
        for(uint32_t i=1; i<=multisigAddrs; i++) {
            auto multisigData = blocksci::script::Multisig(i, btc.getAccess());
            multisigPubkeyCount += multisigData.getTotal();
        }

        std::cout << "multisigPubkeyCount=" << multisigPubkeyCount << std::endl;
        */

        auto inactiveBeforeBlockheight = btc.size() - inactiveAfterMonths * 30 * 24 * 60 / 10;

        // get and store block height of last usage for every address
        std::cout << "### Looking up last usage for every address ###" << std::endl << std::flush;
        AddrUsageMap addressLastUsageBtc;
        addressLastUsageBtc.reserve(btc.getAccess().scripts->totalAddressCount());
        uint32_t txCount = btc.endTxIndex();
        RANGES_FOR(auto block, btc) {
            RANGES_FOR(auto tx, block) {
                RANGES_FOR(auto output, tx.outputs()) {
                    if (output.isSpent()) {
                        continue;
                    }
                    addressLastUsageBtc[{output.getAddress().scriptNum, blocksci::dedupType(output.getAddress().type)}] = block.height();

                    auto addr = output.getAddress();
                    if (blocksci::dedupType(addr.type) == blocksci::DedupAddressType::SCRIPTHASH) {
                        handleScriptHash(addr, block.height(), addressLastUsageBtc, btc);
                    }

                    /*
                    // handle multisig addresses
                    if (blocksci::dedupType(output.getAddress().type) == blocksci::DedupAddressType::MULTISIG) {
                        auto multisigData = blocksci::script::Multisig(output.getAddress().scriptNum, btc.getAccess());
                        auto wrappedAddr = scriptHashData.getWrappedAddress();
                        if (wrappedAddr) {
                            addressLastUsageBtc[{wrappedAddr->scriptNum, blocksci::dedupType(wrappedAddr->type)}] =  block.height();
                        }
                    }
                    */
                }
                RANGES_FOR(auto input, tx.inputs()) {
                    auto addr = input.getAddress();
                    addressLastUsageBtc[{addr.scriptNum, blocksci::dedupType(addr.type)}] = block.height();

                    if (blocksci::dedupType(addr.type) == blocksci::DedupAddressType::SCRIPTHASH) {
                        handleScriptHash(addr, block.height(), addressLastUsageBtc, btc);
                    }

                    /*
                    // handle wrapped addresses
                    if (blocksci::dedupType(input.getAddress().type) == blocksci::DedupAddressType::SCRIPTHASH) {
                        auto scriptHashData = blocksci::script::ScriptHash(input.getAddress().scriptNum, btc.getAccess());
                        auto wrappedAddr = scriptHashData.getWrappedAddress();
                        if (wrappedAddr) {
                            addressLastUsageBtc[{wrappedAddr->scriptNum, blocksci::dedupType(wrappedAddr->type)}] =  block.height();
                        }
                    }
                    */
                }

                // progress bar
                if (tx.txNum % 1000000 == 0) {
                    std::cout << "\rProgress: " << ((float) tx.txNum / txCount) * 100 << "%" << std::flush;
                }
            }
        }

        std::cout << std::endl << std::endl;
        std::cout << "scripts->totalAddressCount()=" << btc.getAccess().scripts->totalAddressCount() << std::endl;
        std::cout << "addressLastUsageBtc.size()=" << addressLastUsageBtc.size() << std::endl;

        uint32_t neverSeenCount = 0;
        uint32_t inactiveCount = 0;
        uint32_t activeCount = 0;
        for (const auto& pair : addressLastUsageBtc) {
            if (pair.second == 0) {
                neverSeenCount++;
            }
            else if (pair.second < inactiveBeforeBlockheight) {
                inactiveCount++;
            }
            else if (pair.second >= inactiveBeforeBlockheight) {
                activeCount++;
            }
        }

        std::cout << "inactiveBeforeBlockheight=" << inactiveBeforeBlockheight << std::endl;
        std::cout << "inactiveCount=" << inactiveCount << std::endl;
        std::cout << "activeCount=" << activeCount << std::endl;
        std::cout << "neverSeenCount=" << neverSeenCount << std::endl;
    }

    return 0;
}
