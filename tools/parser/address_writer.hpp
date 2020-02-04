//
//  address_writer.hpp
//  blocksci
//
//  Created by Harry Kalodner on 7/26/17.
//
//

#ifndef address_writer_hpp
#define address_writer_hpp

#include "script_output.hpp"
#include "script_input.hpp"

template<typename T>
struct ScriptFileType;

template<typename T>
struct ScriptFileType<blocksci::FixedSize<T>> {
    using type = blocksci::FixedSizeFileMapper<T, mio::access_mode::write>;
};

template<typename ...T>
struct ScriptFileType<blocksci::Indexed<T...>> {
    using type = blocksci::IndexedFileMapper<mio::access_mode::write, T...>;
};

template<blocksci::DedupAddressType::Enum type>
using ScriptFileType_t = typename ScriptFileType<typename blocksci::ScriptInfo<type>::storage>::type;

template<blocksci::DedupAddressType::Enum type>
struct ScriptFile : public ScriptFileType_t<type> {
    using ScriptFileType_t<type>::ScriptFileType_t;
};


template<blocksci::DedupAddressType::Enum type>
struct ScriptHeaderFile : public blocksci::FixedSizeFileMapper<blocksci::ScriptHeader, mio::access_mode::write> {
    using FixedSizeFileMapper<blocksci::ScriptHeader, mio::access_mode::write>::FixedSizeFileMapper;
};

class AddressWriter {
    using ScriptFilesTuple = blocksci::to_dedup_address_tuple_t<ScriptFile>;
    using ScriptHeaderFilesTuple = blocksci::to_dedup_address_tuple_t<ScriptHeaderFile>;

    ScriptFilesTuple scriptFiles;
    ScriptHeaderFilesTuple scriptHeaderFiles;

    template<blocksci::AddressType::Enum type>
    void serializeInputImp(const ScriptInput<type> &, ScriptFile<dedupType(type)> &) {}
    
    void serializeInputImp(const ScriptInput<blocksci::AddressType::PUBKEYHASH> &input, ScriptFile<blocksci::DedupAddressType::PUBKEY> &dataFile);
    void serializeInputImp(const ScriptInput<blocksci::AddressType::WITNESS_PUBKEYHASH> &input, ScriptFile<blocksci::DedupAddressType::PUBKEY> &dataFile);
    void serializeInputImp(const ScriptInput<blocksci::AddressType::SCRIPTHASH> &input, ScriptFile<blocksci::DedupAddressType::SCRIPTHASH> &dataFile);
    void serializeInputImp(const ScriptInput<blocksci::AddressType::WITNESS_SCRIPTHASH> &input, ScriptFile<blocksci::DedupAddressType::SCRIPTHASH> &dataFile);
    void serializeInputImp(const ScriptInput<blocksci::AddressType::NONSTANDARD> &input, ScriptFile<blocksci::DedupAddressType::NONSTANDARD> &dataFile);
    void serializeInputImp(const ScriptInput<blocksci::AddressType::WITNESS_UNKNOWN> &input, ScriptFile<blocksci::DedupAddressType::WITNESS_UNKNOWN> &dataFile);
    
    template<blocksci::AddressType::Enum type>
    void serializeOutputImp(const ScriptOutput<type> &output, ScriptFile<dedupType(type)> &, ScriptHeaderFile<dedupType(type)> &headerFile, bool topLevel) {
        auto header = headerFile[output.scriptNum - 1];
        // mark the address as seen for the given address type
        header->saw(type, topLevel);
    }
    
    void serializeOutputImp(const ScriptOutput<blocksci::AddressType::PUBKEY> &output, ScriptFile<blocksci::DedupAddressType::PUBKEY> &dataFile, ScriptHeaderFile<blocksci::DedupAddressType::PUBKEY> &headerFile, bool topLevel);
    void serializeOutputImp(const ScriptOutput<blocksci::AddressType::MULTISIG_PUBKEY> &output, ScriptFile<blocksci::DedupAddressType::PUBKEY> &dataFile, ScriptHeaderFile<blocksci::DedupAddressType::PUBKEY> &headerFile, bool topLevel);
    void serializeOutputImp(const ScriptOutput<blocksci::AddressType::WITNESS_SCRIPTHASH> &output, ScriptFile<blocksci::DedupAddressType::SCRIPTHASH> &dataFile, ScriptHeaderFile<blocksci::DedupAddressType::SCRIPTHASH> &headerFile, bool topLevel);
    void serializeOutputImp(const ScriptOutput<blocksci::AddressType::NONSTANDARD> &output, ScriptFile<blocksci::DedupAddressType::NONSTANDARD> &dataFile, ScriptHeaderFile<blocksci::DedupAddressType::NONSTANDARD> &headerFile, bool topLevel);
    void serializeOutputImp(const ScriptOutput<blocksci::AddressType::WITNESS_UNKNOWN> &output, ScriptFile<blocksci::DedupAddressType::WITNESS_UNKNOWN> &dataFile, ScriptHeaderFile<blocksci::DedupAddressType::WITNESS_UNKNOWN> &headerFile, bool topLevel);
    
    template<blocksci::AddressType::Enum type>
    void serializeWrappedInput(const ScriptInputData<type> &, uint32_t, uint32_t) {}
    
    void serializeWrappedInput(const ScriptInputData<blocksci::AddressType::Enum::SCRIPTHASH> &input, uint32_t txNum, uint32_t outputTxNum);
    void serializeWrappedInput(const ScriptInputData<blocksci::AddressType::Enum::WITNESS_SCRIPTHASH> &input, uint32_t txNum, uint32_t outputTxNum);
    
public:
    
    template <blocksci::DedupAddressType::Enum type>
    ScriptFile<type> &getFile() {
        return std::get<ScriptFile<type>>(scriptFiles);
    }
    
    template <blocksci::DedupAddressType::Enum type>
    const ScriptFile<type> &getFile() const {
        return std::get<ScriptFile<type>>(scriptFiles);
    }

    // add new script to script file
    template<blocksci::AddressType::Enum type>
    blocksci::OffsetType serializeNewOutput(const ScriptOutput<type> &output, uint32_t txNum, bool topLevel) {
        assert(output.isNew);

        auto &dataFile = std::get<ScriptFile<dedupType(type)>>(scriptFiles);
        auto data = output.data.getData();
        dataFile.write(data);
        assert(output.scriptNum == dataFile.size());

        auto &headerFile = std::get<ScriptHeaderFile<dedupType(type)>>(scriptHeaderFiles);
        auto header = output.data.getHeader(txNum, topLevel);
        headerFile.write(header);
        assert(output.scriptNum == headerFile.size());

        // visitWrapped is currently only implemented for ScriptOutputData<blocksci::AddressType::Enum::MULTISIG>
        output.data.visitWrapped([&](auto &wrappedOutput) {
            if (wrappedOutput.isNew) {
                serializeNewOutput(wrappedOutput, txNum, false);
            } else {
                serializeExistingOutput(wrappedOutput, false);
            }
        });
        return dataFile.size();
    }
    
    template<blocksci::AddressType::Enum type>
    void serializeExistingOutput(const ScriptOutput<type> &output, bool topLevel) {
        assert(!output.isNew);
        auto &dataFile = std::get<ScriptFile<dedupType(type)>>(scriptFiles);
        auto &headerFile = std::get<ScriptHeaderFile<dedupType(type)>>(scriptHeaderFiles);
        serializeOutputImp(output, dataFile, headerFile, topLevel);
    }
    
    template<blocksci::AddressType::Enum type>
    void serializeInput(const ScriptInput<type> &input, uint32_t txNum, uint32_t outputTxNum) {
        auto &headerFile = std::get<ScriptHeaderFile<dedupType(type)>>(scriptHeaderFiles);
        auto header = headerFile.getDataAtIndex(input.scriptNum - 1);

        /** Default value of ScriptDataBase.txFirstSpent is (std::numeric_limits<uint32_t>::max()) */
        bool isFirstSpend = header->txFirstSpent == std::numeric_limits<uint32_t>::max();

        /** Default value of ScriptDataBase.txFirstSeen is the txNum of the transaction that contained the script */
        bool isNewerFirstSeen = outputTxNum < header->txFirstSeen;

        if (isNewerFirstSeen) {
            header->txFirstSeen = outputTxNum;
        }
        if (isFirstSpend) {
            auto &file = std::get<ScriptFile<dedupType(type)>>(scriptFiles);
            header->txFirstSpent = txNum;
            serializeInputImp(input, file);
        }
    }
    
    void rollback(const blocksci::State &state);
    
    blocksci::OffsetType serializeNewOutput(const AnyScriptOutput &output, uint32_t txNum, bool topLevel);
    void serializeExistingOutput(const AnyScriptOutput &output, bool topLevel);
    
    void serializeInput(const AnyScriptInput &input, uint32_t txNum, uint32_t outputTxNum);
    void serializeWrappedInput(const AnyScriptInput &input, uint32_t txNum, uint32_t outputTxNum);

    // todo: it is a problem that the firstSeen and firstSpent fields of added ScriptHeader entires is never set
    void fillScriptHeaderFiles() {
        blocksci::for_each(blocksci::DedupAddressType::all(), [&](auto tag) {
            auto &dataFile = std::get<ScriptFile<tag()>>(scriptFiles);
            auto &headerFile = std::get<ScriptHeaderFile<tag()>>(scriptHeaderFiles);
            if (dataFile.size() != headerFile.size()) {
                if (dataFile.size() > headerFile.size()) {
                    uint32_t difference = dataFile.size() - headerFile.size();
                    blocksci::ScriptHeader header{};
                    for (uint32_t i=0; i < difference; i++) {
                        headerFile.write(header);
                    }
                }
                else {
                    throw std::runtime_error("Whoopsie. Something went terribly wrong");
                }
            }
        });
    }
    
    AddressWriter(const ParserConfigurationBase &config);
};

#endif /* address_writer_hpp */
