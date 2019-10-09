//
//  script.cpp
//  blocksci
//
//  Created by Harry Kalodner on 4/10/18.
//

#include <blocksci/scripts/script.hpp>
#include <blocksci/chain/transaction.hpp>

#include <internal/address_info.hpp>
#include <internal/data_access.hpp>
#include <internal/chain_access.hpp>
#include <internal/script_access.hpp>

namespace blocksci {
    
    ScriptBase::ScriptBase(const Address &address)
        : ScriptBase(
            address.scriptNum,
            address.type,
            address.getAccess(),
            address.getAccess().scripts->getScriptHeader(address.scriptNum, dedupType(address.type)),
            address.getAccess().scripts->getScriptData(address.scriptNum, dedupType(address.type))
        ) {}

    ranges::optional<Transaction> ScriptBase::getFirstTransaction() const {
        auto txNum = getFirstTxIndex();
        if (txNum) {
            return Transaction(*txNum, getAccess().getChain().getBlockHeight(*txNum), getAccess());
        }
        else {
            return ranges::nullopt;
        }
    }
    
    ranges::optional<Transaction> ScriptBase::getTransactionRevealed() const {
        auto index = getTxRevealedIndex();
        if (index) {
            return Transaction(*index, getAccess().getChain().getBlockHeight(*index), getAccess());
        } else {
            return ranges::nullopt;
        }
    }
} // namespace blocksci
