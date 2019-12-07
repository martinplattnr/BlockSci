//
//  script_access.hpp
//  blocksci
//
//  Created by Harry Kalodner on 7/26/17.
//
//

#ifndef script_access_hpp
#define script_access_hpp

#include "file_mapper.hpp"
#include "dedup_address_info.hpp"
#include "script_info.hpp"

#include <blocksci/core/meta.hpp>
#include <blocksci/core/script_data.hpp>

#include <wjfilesystem/path.h>

#include <tuple>

namespace blocksci {
    class ScriptAccess;
    
    namespace internal {
        template<DedupAddressType::Enum type>
        struct ScriptCountFunctor {
            static uint32_t f(const ScriptAccess &access);
        };
        
        template<DedupAddressType::Enum type>
        struct ScriptDataBaseFunctor {
            static const ScriptDataBase * f(uint32_t scriptNum, const ScriptAccess &access);
        };

        template<DedupAddressType::Enum type>
        struct ScriptHeaderFunctor {
            static const ScriptHeader* f(uint32_t scriptNum, const ScriptAccess &access);
        };
    } // namespace internal
    
    class ScriptAccess;
    
    template<typename T>
    struct ScriptFileType;
    
    template<typename T>
    struct ScriptFileType<FixedSize<T>> {
        using type = FixedSizeFileMapper<T>;
    };
    
    template<typename ...T>
    struct ScriptFileType<Indexed<T...>> {
        using type = IndexedFileMapper<mio::access_mode::read, T...>;
    };
    
    template<typename T>
    using ScriptFileType_t = typename ScriptFileType<T>::type;
    
    template<DedupAddressType::Enum type>
    using ScriptFile = ScriptFileType_t<typename ScriptInfo<type>::storage>;



    template<DedupAddressType::Enum type>
    struct ScriptHeaderFile : public FixedSizeFileMapper<ScriptHeader> {
        using FixedSizeFileMapper<ScriptHeader>::FixedSizeFileMapper;
    };

    /** Provides access to script data of all address types
     *
     * BlockSci supports the parsing of all standard Bitcoin address types in order to extract relevant data.
     * Each address type has its own file/s storing this data.
     *
     * Directory: scripts/
     */
    class ScriptAccess {
    private:
        using ScriptFilesTuple = to_dedup_address_tuple_t<ScriptFile>;
        ScriptFilesTuple scriptFiles;

        using ScriptHeaderFilesTuple = to_dedup_address_tuple_t<ScriptHeaderFile>;
        ScriptHeaderFilesTuple scriptHeaderFiles;


        template<DedupAddressType::Enum type>
        friend struct internal::ScriptHeaderFunctor;

        template<DedupAddressType::Enum type>
        const ScriptHeader* getScriptHeaderImpl(uint32_t scriptNum) const {
            auto &file = std::get<ScriptHeaderFile<type>>(scriptHeaderFiles);
            return file[scriptNum - 1];
        }

    public:
        explicit ScriptAccess(const filesystem::path &rootDirectory, const filesystem::path &localDirectory) :
        scriptFiles(blocksci::apply(DedupAddressType::all(), [&] (auto tag) {
            return ScriptFile<tag.value>{rootDirectory/std::string{dedupAddressName(tag)}};
        })),
        scriptHeaderFiles(blocksci::apply(DedupAddressType::all(), [&] (auto tag) {
            return ScriptHeaderFile<tag.value>{localDirectory/std::string{dedupAddressName(tag) + "_header"}};
        })) {}

        template <DedupAddressType::Enum type>
        ScriptFile<type> &getFile() {
            return *std::get<ScriptFile<type>>(scriptFiles);
        }
        
        template <DedupAddressType::Enum type>
        const ScriptFile<type> &getFile() const {
            return std::get<ScriptFile<type>>(scriptFiles);
        }

        template <DedupAddressType::Enum type>
        ScriptHeaderFile<type> &getHeaderFile() {
            return *std::get<ScriptHeaderFile<type>>(scriptHeaderFiles);
        }

        template <DedupAddressType::Enum type>
        const ScriptHeaderFile<type> &getHeaderFile() const {
            return std::get<ScriptHeaderFile<type>>(scriptHeaderFiles);
        }

        const ScriptHeader* getScriptHeader(uint32_t addressNum, DedupAddressType::Enum type) const {
            static auto table = blocksci::make_dynamic_table<DedupAddressType, internal::ScriptHeaderFunctor>();
            auto index = static_cast<size_t>(type);
            return table.at(index)(addressNum, *this);
        }
        
        template <DedupAddressType::Enum type>
        auto getScriptData(uint32_t addressNum) const {
            return getFile<type>()[addressNum - 1];
        }
        
        const ScriptDataBase *getScriptData(uint32_t addressNum, DedupAddressType::Enum type) const {
            static auto &scriptDataBaseTable = *[]() {
                auto table = make_dynamic_table<DedupAddressType, internal::ScriptDataBaseFunctor>();
                return new decltype(table){table};
            }();
            auto index = static_cast<size_t>(type);
            return scriptDataBaseTable.at(index)(addressNum, *this);
        }

        // todo-fork: gives chain-family results if script files are shared among forks
        std::array<uint32_t, DedupAddressType::size> scriptCounts() const {
            return make_static_table<DedupAddressType, internal::ScriptCountFunctor>(*this);
        }

        // todo-fork: gives chain-family results if script files are shared among forks
        uint32_t scriptCount(DedupAddressType::Enum type) const {
            static constexpr auto table = make_dynamic_table<DedupAddressType, internal::ScriptCountFunctor>();
            auto index = static_cast<size_t>(type);
            return table.at(index)(*this);
        }

        // todo-fork: gives chain-family results if script files are shared among forks
        size_t totalAddressCount() const {
            uint32_t count = 0;
            for_each(scriptFiles, [&count](auto& obj) -> decltype(auto) {count += obj.size();});
            return count;
        }
        
        void reload() {
            for_each(scriptFiles, [&](auto& file) -> decltype(auto) { file.reload(); });
        }
    };
        
    namespace internal {
        template<DedupAddressType::Enum type>
        uint32_t ScriptCountFunctor<type>::f(const ScriptAccess &access) {
            return static_cast<uint32_t>(access.getFile<type>().size());
        }
        
        template<DedupAddressType::Enum type>
        const ScriptDataBase * ScriptDataBaseFunctor<type>::f(uint32_t scriptNum, const ScriptAccess &access) {
            auto &file = access.getFile<type>();
            return file.getDataAtIndex(scriptNum - 1);
        }

        template<DedupAddressType::Enum type>
        const ScriptHeader* ScriptHeaderFunctor<type>::f(uint32_t scriptNum, const ScriptAccess &access) {
            return access.getScriptHeaderImpl<type>(scriptNum);
        }
    } // namespace internal
    
} // namespace blocksci

#endif /* script_access_hpp */
