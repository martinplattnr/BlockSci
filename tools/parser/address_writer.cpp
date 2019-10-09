//
//  address_writer.cpp
//  blocksci
//
//  Created by Harry Kalodner on 7/26/17.
//
//
#define BLOCKSCI_WITHOUT_SINGLETON

#include <blocksci/scripts/bitcoin_pubkey.hpp>
#include "address_writer.hpp"
#include "preproccessed_block.hpp"

using blocksci::AddressType;
using blocksci::DedupAddressType;

AddressWriter::AddressWriter(const ParserConfigurationBase &config) :
scriptFiles(blocksci::apply(DedupAddressType::all(), [&] (auto tag) {
    return (filesystem::path{config.dataConfig.rootScriptsDirectory()}/std::string{dedupAddressName(tag)}).str();
})),
scriptHeaderFiles(blocksci::apply(DedupAddressType::all(), [&] (auto tag) {
    return (filesystem::path{config.dataConfig.scriptsDirectory()}/std::string{dedupAddressName(tag) + "_header"}).str();
})) {
}

blocksci::OffsetType AddressWriter::serializeNewOutput(const AnyScriptOutput &output, uint32_t txNum, bool topLevel) {
    return mpark::visit([&](auto &scriptOutput) { return this->serializeNewOutput(scriptOutput, txNum, topLevel); }, output.wrapped);
}

void AddressWriter::serializeExistingOutput(const AnyScriptOutput &output, bool topLevel) {
    mpark::visit([&](auto &scriptOutput) { return this->serializeExistingOutput(scriptOutput, topLevel); }, output.wrapped);
}

void AddressWriter::serializeInput(const AnyScriptInput &input, uint32_t txNum, uint32_t outputTxNum) {
    mpark::visit([&](auto &scriptInput) { this->serializeInput(scriptInput, txNum, outputTxNum); }, input.wrapped);
}

void AddressWriter::serializeWrappedInput(const AnyScriptInput &input, uint32_t txNum, uint32_t outputTxNum) {
    mpark::visit([&](auto &scriptInput) { this->serializeWrappedInput(scriptInput.data, txNum, outputTxNum); }, input.wrapped);
}

void AddressWriter::serializeOutputImp(const ScriptOutput<AddressType::PUBKEY> &output, ScriptFile<DedupAddressType::PUBKEY> &dataFile, ScriptHeaderFile<DedupAddressType::PUBKEY> &headerFile, bool topLevel) {
    auto data = dataFile[output.scriptNum - 1];
    auto header = headerFile[output.scriptNum - 1];

    // determine length of public key and only copy relevant bytes
    auto itBegin = output.data.pubkey.begin();
    auto itEnd = itBegin + blocksci::CPubKey::GetLen(output.data.pubkey[0]);
    std::copy(itBegin, itEnd, data->pubkey.begin());

    data->hasPubkey = true;
    header->saw(AddressType::PUBKEY, topLevel);
}

void AddressWriter::serializeOutputImp(const ScriptOutput<AddressType::MULTISIG_PUBKEY> &output, ScriptFile<DedupAddressType::PUBKEY> &dataFile, ScriptHeaderFile<DedupAddressType::PUBKEY> &headerFile, bool topLevel) {
    auto data = dataFile[output.scriptNum - 1];
    auto header = headerFile[output.scriptNum - 1];

    std::copy(output.data.pubkey.begin(), output.data.pubkey.end(), data->pubkey.begin());
    header->saw(AddressType::MULTISIG_PUBKEY, topLevel);
    data->hasPubkey = true;
}

void AddressWriter::serializeOutputImp(const ScriptOutput<AddressType::WITNESS_SCRIPTHASH> &output, ScriptFile<DedupAddressType::SCRIPTHASH> &dataFile, ScriptHeaderFile<DedupAddressType::SCRIPTHASH> &headerFile, bool topLevel) {
    auto data = dataFile[output.scriptNum - 1];
    auto header = headerFile[output.scriptNum - 1];

    data->hash256 = output.data.hash;
    data->isSegwit = true;
    header->saw(AddressType::WITNESS_SCRIPTHASH, topLevel);
}

void AddressWriter::serializeOutputImp(const ScriptOutput<AddressType::NONSTANDARD> &output, ScriptFile<DedupAddressType::NONSTANDARD> &, ScriptHeaderFile<DedupAddressType::NONSTANDARD> &headerFile, bool topLevel) {
    auto header = headerFile[output.scriptNum - 1];
    header->saw(AddressType::NONSTANDARD, topLevel);
}
void AddressWriter::serializeOutputImp(const ScriptOutput<AddressType::WITNESS_UNKNOWN> &output, ScriptFile<DedupAddressType::WITNESS_UNKNOWN> &, ScriptHeaderFile<DedupAddressType::WITNESS_UNKNOWN> &headerFile, bool topLevel) {
    auto header = headerFile[output.scriptNum - 1];
    header->saw(AddressType::WITNESS_UNKNOWN, topLevel);
}

void AddressWriter::serializeInputImp(const ScriptInput<AddressType::PUBKEYHASH> &input, ScriptFile<DedupAddressType::PUBKEY> &dataFile) {
    auto data = dataFile[input.scriptNum - 1];
    if (!data->hasPubkey) {
        // determine length of public key and only copy relevant bytes
        auto itBegin = input.data.pubkey.begin();
        auto itEnd = itBegin + blocksci::CPubKey::GetLen(input.data.pubkey[0]);;
        std::copy(itBegin, itEnd, data->pubkey.begin());

        data->hasPubkey = true;
    }
}

void AddressWriter::serializeInputImp(const ScriptInput<AddressType::WITNESS_PUBKEYHASH> &input, ScriptFile<DedupAddressType::PUBKEY> &dataFile) {
    auto data = dataFile[input.scriptNum - 1];
    if (!data->hasPubkey) {
        std::copy(input.data.pubkey.begin(), input.data.pubkey.end(), data->pubkey.begin());
        data->hasPubkey = true;
    }
}

void AddressWriter::serializeInputImp(const ScriptInput<AddressType::WITNESS_SCRIPTHASH> &input, ScriptFile<DedupAddressType::SCRIPTHASH> &dataFile) {
    auto data = dataFile[input.scriptNum - 1];
    data->wrappedAddress = input.data.wrappedScriptOutput.address();
}

void AddressWriter::serializeWrappedInput(const ScriptInputData<AddressType::Enum::WITNESS_SCRIPTHASH> &data, uint32_t txNum, uint32_t outputTxNum) {
    if (data.wrappedScriptOutput.isNew()) {
        serializeNewOutput(data.wrappedScriptOutput, txNum, false);
    } else {
        serializeExistingOutput(data.wrappedScriptOutput, false);
    }
    
    serializeInput(*data.wrappedScriptInput, txNum, outputTxNum);
}

void AddressWriter::serializeInputImp(const ScriptInput<AddressType::SCRIPTHASH> &input, ScriptFile<DedupAddressType::SCRIPTHASH> &dataFile) {
    auto data = dataFile[input.scriptNum - 1];
    data->wrappedAddress = input.data.wrappedScriptOutput.address();
}

void AddressWriter::serializeWrappedInput(const ScriptInputData<AddressType::Enum::SCRIPTHASH> &data, uint32_t txNum, uint32_t outputTxNum) {
    if (data.wrappedScriptOutput.isNew()) {
        serializeNewOutput(data.wrappedScriptOutput, txNum, false);
    } else {
        serializeExistingOutput(data.wrappedScriptOutput, false);
    }
    serializeInput(*data.wrappedScriptInput, txNum, outputTxNum);
    serializeWrappedInput(*data.wrappedScriptInput, txNum, outputTxNum);
}

void AddressWriter::serializeInputImp(const ScriptInput<AddressType::NONSTANDARD> &input, ScriptFile<DedupAddressType::NONSTANDARD> &dataFile) {
    blocksci::NonstandardSpendScriptData scriptData(static_cast<uint32_t>(input.data.script.size()));
    blocksci::ArbitraryLengthData<blocksci::NonstandardSpendScriptData> data(scriptData);
    data.add(input.data.script.begin(), input.data.script.end());
    dataFile.write<1>(input.scriptNum - 1, data);
}

void AddressWriter::serializeInputImp(const ScriptInput<AddressType::WITNESS_UNKNOWN> &input, ScriptFile<DedupAddressType::WITNESS_UNKNOWN> &dataFile) {
    blocksci::WitnessUnknownSpendScriptData scriptData(static_cast<uint32_t>(input.data.script.size()));
    blocksci::ArbitraryLengthData<blocksci::WitnessUnknownSpendScriptData> data(scriptData);
    data.add(input.data.script.begin(), input.data.script.end());
    dataFile.write<1>(input.scriptNum - 1, data);
}

void AddressWriter::rollback(const blocksci::State &state) {
    blocksci::for_each(DedupAddressType::all(), [&](auto tag) {
        auto &file = std::get<ScriptFile<tag()>>(scriptFiles);
        file.truncate(state.scriptCounts[static_cast<size_t>(tag)]);
    });
}
