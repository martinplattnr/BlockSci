//
// Created by martin on 04.02.20.
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
    blocksci::Blockchain ltc{"/mnt/data/blocksci/ltc/2019-12-31/config.json"};
    blocksci::Blockchain btcCc1{"/mnt/data/blocksci/btc-bch/2019-12-31/btc/config.json"};

    // SCRIPTHASH ADDRESSES (incl. witness script hash)
    uint32_t commonScriptHashAddresses = 0;
    auto dedupType = blocksci::DedupAddressType::Enum::SCRIPTHASH;
    auto scriptCount = ltc.getAccess().scripts->scriptCount(dedupType);
    std::cout << std::endl << "comparing " << scriptCount << "scripthash addresses:" << std::endl;
    for (uint32_t i = 1; i <= scriptCount; ++i) {
        auto anyScript = blocksci::Address{i, blocksci::reprType(dedupType), ltc.getAccess()}.getScript();
        auto scriptHashInLtc = mpark::get<blocksci::ScriptAddress<blocksci::AddressType::SCRIPTHASH>>(
                anyScript.wrapped);

        auto scriptNum1InBtcCc1 = btcCc1.getAccess().hashIndex->getScriptHashIndex(
                scriptHashInLtc.getUint160Address()); // SCRIPTHASH
        if (scriptNum1InBtcCc1 && *scriptNum1InBtcCc1) {
            ++commonScriptHashAddresses;
        } else {
            auto scriptNum2InBtcCc1 = btcCc1.getAccess().hashIndex->getScriptHashIndex(
                    scriptHashInLtc.getUint256Address()); // WITNESS_SCRIPTHASH
            if (scriptNum2InBtcCc1 && *scriptNum2InBtcCc1) {
                ++commonScriptHashAddresses;
            }
        }
        if (i % 10000 == 0) {
            std::cout << "\r" << ((float) i / scriptCount) * 100 << "% (Result: " << commonScriptHashAddresses << ")"
                      << std::flush;
        }
    }
    std::cout << "commonScriptHashAddresses=" << commonScriptHashAddresses << std::endl << std::endl << std::endl;

    // PUBKEY ADDRESSES (all types: P2PKH, P2PK, witness/non-witness)
    uint32_t commonPubkeyAddresses = 0;
    dedupType = blocksci::DedupAddressType::Enum::PUBKEY;
    scriptCount = ltc.getAccess().scripts->scriptCount(dedupType);
    std::cout << std::endl << "comparing " << scriptCount << " pubkey addresses:" << std::endl;
    for (uint32_t i = 1; i <= scriptCount; ++i) {
        auto anyScript = blocksci::Address{i, blocksci::reprType(dedupType), ltc.getAccess()}.getScript();
        auto pubkeyInLtc = mpark::get<blocksci::ScriptAddress<blocksci::AddressType::PUBKEYHASH>>(anyScript.wrapped);

        auto scriptNumInBtcCc1 = btcCc1.getAccess().hashIndex->getPubkeyHashIndex(pubkeyInLtc.getPubkeyHash());
        if (scriptNumInBtcCc1 && *scriptNumInBtcCc1) {
            ++commonPubkeyAddresses;
        }
        if (i % 10000 == 0) {
            std::cout << "\r" << ((float) i / scriptCount) * 100 << "% (Result: " << commonPubkeyAddresses << ")"
                      << std::flush;
        }
    }
    std::cout << "commonPubkeyAddresses=" << commonPubkeyAddresses << std::endl << std::endl << std::endl;

    return 0;
}
