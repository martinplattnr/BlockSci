//
//  address_state.cpp
//  blocksci
//
//  Created by Harry Kalodner on 9/10/17.
//
//

#include "address_state.hpp"
#include "parser_configuration.hpp"

#include <blocksci/core/bitcoin_uint256.hpp>

#include <internal/address_info.hpp>

#include <fstream>
#include <string>
#include <sstream>

namespace {
    static constexpr auto multiAddressFileName = "multi";
    static constexpr auto bloomFileName = "bloom_";
    static constexpr auto scriptCountsFileName = "scriptCounts.txt";
}

AddressState::AddressState(filesystem::path localDirectory_, filesystem::path rootDirectory_, HashIndexCreator &hashDb) : localDirectory(std::move(localDirectory_)), rootDirectory(std::move(rootDirectory_)), db(hashDb), addressBloomFilters(blocksci::apply(blocksci::DedupAddressType::all(), [&] (auto tag) {
    return std::make_unique<AddressBloomFilter<tag>>(rootDirectory/std::string(bloomFileName));
})) {
    // use local data for the multi-address-maps
    blocksci::for_each(multiAddressMaps, [&](auto &multiAddressMap) {
        std::stringstream ss;
        ss << multiAddressFileName << "_" << dedupAddressName(multiAddressMap.type) << ".dat";
        // the multi address map files are stored locally and per-chain, in the chain's output directory, not in the root chain's directory
        multiAddressMap.unserialize((localDirectory/ss.str()).str());
    });

    // use the root scriptCounts.txt file
    std::ifstream inputFile((rootDirectory/std::string(scriptCountsFileName)).str());

    if (inputFile) {
        uint32_t value;
        while ( inputFile >> value ) {
            scriptIndexes.push_back(value);
        }
    } else {
        for (size_t i = 0; i < blocksci::DedupAddressType::size; i++) {
            scriptIndexes.push_back(1);
        }
    }
}

AddressState::~AddressState() {
    blocksci::for_each(multiAddressMaps, [&](auto &multiAddressMap) {
        std::stringstream ss;
        ss << multiAddressFileName << "_" << dedupAddressName(multiAddressMap.type) << ".dat";
        // the multi address map files are stored locally and per-chain, in the chain's output directory, not in the root chain's directory
        multiAddressMap.serialize((localDirectory/ss.str()).str());
    });

    // the scripts-counts file is stored once for all related (forked) chains
    std::ofstream outputFile((rootDirectory/std::string(scriptCountsFileName)).str());
    for (auto value : scriptIndexes) {
        outputFile << value << " ";
    }
}

uint32_t AddressState::getNewAddressIndex(blocksci::DedupAddressType::Enum type) {
    auto &count = scriptIndexes[static_cast<uint8_t>(type)];
    auto scriptNum = count;
    count++;
    return scriptNum;
}

void AddressState::rollback(const blocksci::State &state) {
    blocksci::for_each(multiAddressMaps, [&](auto &multiAddressMap) {
        for (auto multiAddressIt = multiAddressMap.begin(); multiAddressIt != multiAddressMap.end(); ++multiAddressIt) {
            auto count = state.scriptCounts[static_cast<size_t>(multiAddressMap.type)];
            if (multiAddressIt->second >= count) {
                multiAddressMap.erase(multiAddressIt);
            }
        }
    });
}

void AddressState::reset(const blocksci::State &state) {
    reloadBloomFilters();
    scriptIndexes.clear();
    for (auto size : state.scriptCounts) {
        scriptIndexes.push_back(size);
    }
}
