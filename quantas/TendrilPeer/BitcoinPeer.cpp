// Ben Leber
// 5/20/2026
// Bitcoin Peer Implementation

#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <set>
#include <iterator>
#include <utility>

#include "BitcoinPeer.hpp"
#include "../Common/Peer.hpp"

namespace quantas {
    static bool registerBitcoinPeer = []() {
        return PeerRegistry::registerPeerType("BitcoinPeer", [](interfaceId pubId) { 
            return new BitcoinPeer(new NetworkInterfaceAbstract(pubId)); 
        });
    }();

    BitcoinPeer::BitcoinPeer(NetworkInterface* interfacePtr) : Peer(interfacePtr) {
        // sets random chance for specific node
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
    } 
    BitcoinPeer::BitcoinPeer(const BitcoinPeer& rhs) : Peer(rhs) {}

    void BitcoinPeer::initParameters(const std::vector<Peer*>& peers, json parameters) {
        std::vector<BitcoinPeer*> bpeers = reinterpret_cast<const std::vector<BitcoinPeer*>&>(peers);
        // mining probability
        // trnsxn probability
        
    }

    void BitcoinPeer::performComputation() {
        // check for incoming messages
        checkInStream();

        // attempt to mine the head block
        attemptMine();

        // attempt to introduce a new transaction
        attemptTxn();
    }

    void BitcoinPeer::endOfRound(std::vector<Peer*>& peers) {
        std::vector<BitcoinPeer*> bpeers = reinterpret_cast<std::vector<BitcoinPeer*>&>(peers);
        Logger::log(LogLevel::Debug, "endOfRound()", "End of Round " + std::to_string(RoundManager::currentRound()) + ".");
        // count transactions made
        int txsMade = 0;
        Logger::log(LogLevel::Debug, "endOfRound()", "\t\tTransactions made: " + std::to_string(txsMade));

        // count messages sent
        int msgsSent = 0;
        Logger::log(LogLevel::Debug, "endOfRound()", "\t\tMessages sent: " + std::to_string(msgsSent));

        // count blocks mined
    }

    void BitcoinPeer::endOfExperiment(std::vector<Peer*>& peers) {
        std::vector<BitcoinPeer*> bpeers = reinterpret_cast<std::vector<BitcoinPeer*>&>(peers);

    }

    Block BitcoinPeer::createNewBlock() {
        Block b;
        b.id = blocksMined_;
        b.prevId = tip_.id;
        b.roundMined = -1;
        b.miner = publicId();
        // insert the new coinbase transaction
        b.txns.insert(createNewTransaction(-1, publicId()));
        ++blocksMined_;
        return b;
    }

    Transaction BitcoinPeer::createNewTransaction(const interfaceId& sourceId, const interfaceId& receiverId) {
        Transaction t;
        t.id = txnsMade_;
        t.roundCreated = RoundManager::currentRound();
        t.source = sourceId;
        t.receiver = receiverId;
        ++txnsMade_;
        return t;
    }

    void BitcoinPeer::attemptMine() {
        int random = rand() % 100 + 1;
        if (random >= 100 - (100 * mineProbability_)) {
            // block has been mined
            // build header message
        }
    }

    void BitcoinPeer::attemptTxn() {
        int random = rand() % 100 + 1;
        // transaction probability is hit
        if (random >= 100 - (100 * txnProbability_)) {
            Transaction newTx;
            std::set<interfaceId> neighborSet = neighbors();
            if (!neighborSet.empty()) {
                int index = rand() % neighbors().size();
                auto it = neighborSet.begin();
                std::advance(it, index);
                newTx = createNewTransaction(publicId(), *it);
            }
            else {
                newTx = createNewTransaction(publicId(), -1);
            }
            json newTxMsg = buildTxnMsg(newTx);
            broadcast(newTxMsg);
        }
    }

    Block BitcoinPeer::hasBlock(const Block& b) const {
        auto it = knownBlocks_.find(b);
        if (it != knownBlocks_.end()) {
            return *it;
        }
        else {
            Block emptyBlock;
            emptyBlock.id = -2;
            return emptyBlock;
        }
    }

    Transaction BitcoinPeer::hasTxn(const Transaction& t) const {
        auto it = mempool_.find(t);
        if (it != mempool_.end()) {
            return *it;
        }
        else {
            Transaction emptyTxn;
            emptyTxn.id = -2;
            return emptyTxn;
        }
    }

    void BitcoinPeer::checkInStream() {
        while (!inStreamEmpty()) {
            Packet p = popInStream();
            json msg = p.getMessage();
            // deal with specifics based on the message type
            if (msg["type"] == MessageType::HEADER) {

            }
            else if (msg["type"] == MessageType::GET_DATA) {
                
            }
            else if (msg["type"] == MessageType::CMP_BLOCK) {
                
            }
            else if (msg["type"] == MessageType::GET_BLOCK_TXN) {
                
            }
            else if (msg["type"] == MessageType::BLOCK_TXN) {
                
            }
            else if (msg["type"] == MessageType::TXN) {
                
            }
        }
    }

    // requires: block_id, prev_id, round_mined, miner_id
    json BitcoinPeer::buildHeaderMsg(const Block& b) const {
        json msg;    
        msg["type"] = MessageType::HEADER;
        msg["content"]["block_id"] = b.id;
        msg["content"]["prev_id"] = b.prevId;
        msg["content"]["round_mined"] = b.roundMined;
        msg["content"]["miner_id"] = b.miner;
        return msg;
    }

    // requires: block_id_requested
    json BitcoinPeer::buildGetDataMsg(const int& id) const {
        json msg;
        msg["type"] = MessageType::GET_DATA;
        msg["content"]["block_id_requested"] = id;
        return msg;
    }

    // requires: block_id, prev_id, round_mined, miner_id, tx_ids
    json BitcoinPeer::buildCmpBlockMsg(const Block& b) const {
        json msg;
        msg["type"] = MessageType::CMP_BLOCK;
        msg["content"]["block_id"] = b.id;
        msg["content"]["prev_id"] = b.prevId;
        msg["content"]["round_mined"] = b.roundMined;
        msg["content"]["miner_id"] = b.miner;
        for (const auto& t : b.txns) {
            msg["content"]["tx_ids"] += t.id;
        }
        return msg;
    }

    // requires: block_tx_requested
    json BitcoinPeer::buildGetBlockTxnMsg(const int& id) const {
        json msg;
        msg["type"] = MessageType::GET_BLOCK_TXN;
        msg["content"]["block_txn_requested"] = id;
        return msg;
    }

    // requires: txs (bunch of TXN messages)
    json BitcoinPeer::buildBlockTxnMsg(const Block& b) const {
        json msg;
        msg["type"] = MessageType::BLOCK_TXN;
        msg["content"]["block_id"] = b.id;
        for (const auto& t : b.txns) {
            msg["content"]["txs"] += buildTxnMsg(t);
        }
        return msg;
    }

    // requires: tx_id, round_created, source_id, receiver_id
    json BitcoinPeer::buildTxnMsg(const Transaction& t) const {
        json msg;
        msg["type"] = MessageType::TXN;
        msg["content"]["tx_id"] = t.id;
        msg["content"]["round_created"] = t.roundCreated;
        msg["content"]["source_id"] = t.source;
        msg["content"]["receiver_id"] = t.receiver;
        return msg;
    }

    Block BitcoinPeer::buildBlockFromMsg(const json& msg) const {
        Block result;
        return result;
    }

    Transaction BitcoinPeer::buildTxnFromMsg(const json& msg) const {
        Transaction result;
        return result;
    }

    

}