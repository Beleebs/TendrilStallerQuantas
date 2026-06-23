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
        // mining probability
        // trnsxn probability
        
    }

    void BitcoinPeer::performComputation() {
        // check for incoming messages
        checkInStream();

        // attempt to mine the head block
        attemptMine();

        // attempt to introduce a new transaction
        attemptTx();
    }

    void BitcoinPeer::endOfRound(std::vector<Peer*>& peers) {
        Logger::log(LogLevel::Debug, "endOfRound()", "End of Round " + std::to_string(RoundManager::currentRound()) + ".");
        // count transactions made

        Logger::log(LogLevel::Debug, "endOfRound()", "\t\tTransactions made: ");

        // count messages sent

        // count blocks mined
    }

    void BitcoinPeer::endOfExperiment(std::vector<Peer*>& peers) {
        
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
        for (const auto& t : b.txs) {
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
        for (const auto& t : b.txs) {
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

    Block BitcoinPeer::createNewBlock() {
        Block b;
        b.id = blocksMined_;
        b.prevId = tipId_;
        b.roundMined = -1;
        b.miner = publicId();
        // insert the new coinbase transaction
        b.txs.insert(createNewTransaction(-1, publicId()));

        ++blocksMined_;
        return b;
    }

    Transaction BitcoinPeer::createNewTransaction(const interfaceId& sourceId, const interfaceId& receiverId) {
        Transaction t;
        t.id = txsMade_;
        t.roundCreated = RoundManager::currentRound();
        t.source = sourceId;
        t.receiver = receiverId;
        ++txsMade_;
        return t;
    }

    

    void BitcoinPeer::attemptMine() {
        int random = rand() % 100 + 1;
        if (random >= 100 - (100 * mineProbability_)) {
            // block has been mined
            // build header message
        }
    }

    void BitcoinPeer::attemptTx() {
        int random = rand() % 100 + 1;
        // transaction probability is hit
        if (random >= 100 - (100 * txProbability_)) {
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

}