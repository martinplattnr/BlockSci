//
// Created by martin on 01.10.19.
//

#include <blocksci/chain/input_range.hpp>
#include <internal/data_access.hpp>

namespace blocksci {
    ChainId::Enum getChainId(DataAccess* da) {
        return da->chainId;
    }
}
