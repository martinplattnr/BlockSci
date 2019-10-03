//
//  inout_pointer.hpp
//  blocksci
//
//  Created by Harry Kalodner on 9/4/17.
//
//

#ifndef inout_pointer_hpp
#define inout_pointer_hpp

#include <blocksci/blocksci_export.h>
#include <blocksci/core/hash_combine.hpp>

#include <cstdint>
#include <functional>
#include <blocksci/core/chain_ids.hpp>

namespace blocksci {
    /** Contains data to uniquely identify one input or output using txNum and inoutNum */
    struct BLOCKSCI_EXPORT InoutPointer {
        ChainId::Enum chainId;
        uint32_t txNum;
        uint16_t inoutNum;


        InoutPointer() : chainId(ChainId::UNSPECIFIED), txNum(0), inoutNum(0) {}
        InoutPointer(uint32_t txNum_, uint16_t inoutNum_) : chainId(ChainId::UNSPECIFIED), txNum(txNum_), inoutNum(inoutNum_) {}
        InoutPointer(ChainId::Enum chainId_, uint32_t txNum_, uint16_t inoutNum_) : chainId(chainId_), txNum(txNum_), inoutNum(inoutNum_) {}

        bool operator==(const InoutPointer& other) const {
            return chainId == other.chainId && txNum == other.txNum && inoutNum == other.inoutNum;
        }
        
        bool operator!=(const InoutPointer& other) const {
            return chainId != other.chainId || txNum != other.txNum || inoutNum != other.inoutNum;
        }
        
        bool operator<(const InoutPointer& other) const {
            return std::tie(chainId, txNum, inoutNum) < std::tie(other.chainId, other.txNum, other.inoutNum);
        }
        
        bool operator<=(const InoutPointer& other) const {
            return std::tie(chainId, txNum, inoutNum) <= std::tie(other.chainId, other.txNum, other.inoutNum);
        }
        
        bool operator>(const InoutPointer& other) const {
            return std::tie(chainId, txNum, inoutNum) > std::tie(other.chainId, other.txNum, other.inoutNum);
        }
        
        bool operator>=(const InoutPointer& other) const {
            return std::tie(chainId, txNum, inoutNum) >= std::tie(other.chainId, other.txNum, other.inoutNum);
        }
    };
} // namespace blocksci

namespace std {
    template<> struct BLOCKSCI_EXPORT hash<blocksci::InoutPointer> {
        size_t operator()(const blocksci::InoutPointer &pointer) const {
            std::size_t seed = 41352363;
            blocksci::hash_combine(seed, pointer.chainId);
            blocksci::hash_combine(seed, pointer.txNum);
            blocksci::hash_combine(seed, pointer.inoutNum);
            return seed;
        }
    };
} // namespace std

#endif /* inout_pointer_hpp */
