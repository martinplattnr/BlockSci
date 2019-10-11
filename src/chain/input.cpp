//
//  raw_input.cpp
//  blocksci_devel
//
//  Created by Harry Kalodner on 3/2/17.
//  Copyright © 2017 Harry Kalodner. All rights reserved.
//

#define BLOCKSCI_WITHOUT_SINGLETON

#include <blocksci/chain/input.hpp>
#include <blocksci/chain/block.hpp>
#include <blocksci/address/address.hpp>

#include <internal/chain_access.hpp>
#include <internal/data_access.hpp>

#include <sstream>
#include <stdexcept>

namespace blocksci {
    Input::Input(const InputPointer &pointer_, DataAccess &access_) {
        if (pointer_.chainId != access_.chainId) {
            throw std::runtime_error("This method currently only supports single-chain access");
        }

        access = &access_;
        maxTxCount = static_cast<uint32_t>(access_.getChain().txCount());
        inout = &access_.getChain().getTx(pointer_.txNum)->getInput(pointer_.inoutNum);
        spentOutputNum = &access_.getChain().getSpentOutputNumbers(pointer_.txNum)[pointer_.inoutNum];
        sequenceNum = &access_.getChain().getSequenceNumbers(pointer_.txNum)[pointer_.inoutNum];
        pointer = pointer_;
        blockHeight = access_.getChain().getBlockHeight(pointer_.txNum);
    }
    
    Transaction Input::transaction() const {
        return {pointer.txNum, blockHeight, *access};
    }
    
    BlockHeight Input::age() const {
        return blockHeight - access->getChain().getBlockHeight(inout->getLinkedTxNum());
    }
    
    Block Input::block() const {
        return {blockHeight, *access};
    }
    
    Address Input::getAddress() const {
        return {inout->getAddressNum(), inout->getType(), *access};
    }
    
    Transaction Input::getSpentTx() const {
        auto txNum = inout->getLinkedTxNum();
        return {txNum, access->getChain().getBlockHeight(txNum), *access};
    }
    
    Output Input::getSpentOutput() const {
        auto pointer = getSpentOutputPointer();
        auto height = access->getChain().getBlockHeight(pointer.txNum);
        auto tx = access->getChain().getTx(pointer.txNum);
        return {pointer, height, tx->getOutput(pointer.inoutNum), maxTxCount, *access};
    }
    
    std::string Input::toString() const {
        std::stringstream ss;
        ss << "TxIn(spent_tx_index=" << inout->getLinkedTxNum() << ", address=" << getAddress().toString() <<", value=" << inout->getValue() << ")";
        return ss.str();
    }

    OutputPointer Input::getSpentOutputPointer() const {
        return {access->chainId, inout->getLinkedTxNum(), *spentOutputNum};
    }

    std::ostream &operator<<(std::ostream &os, const Input &input) {
        return os << input.toString();
    }
} // namespace blocksci
