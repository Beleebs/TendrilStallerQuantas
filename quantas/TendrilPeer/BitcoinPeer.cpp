// Ben Leber
// 5/20/2026
// Implementation based off of the v0.10.0 version of Bitcoin 

#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>
#include <ctime>

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

    // sets the parameters for each node
    void BitcoinPeer::initParameters(const std::vector<Peer*>& peers, json parameters) {
        const std::vector<BitcoinPeer*>& bp = reinterpret_cast<const std::vector<BitcoinPeer*>&>(peers); 

        // init the genesis block
        Bk genesis;
        genesis.id = 1;
        genesis.height = 0;
        genesis.prevID = NULL;
        genesis.roundMined = 1;
        genesis.numTx = 1;
        genesis.txs = { 
            {1, 1} 
        };
        genesis.cbTx = *genesis.txs.begin();

        // setup each node
        for (auto p : bp) {
            // change the probability values
            p->mineProbability_ = parameters.value("mineProbability", mineProbability_);
            p->txProbability_ = parameters.value("txProbability", txProbability_);
            
            // set the top block ID to the genesis node
            p->topBlockID_ = genesis.id;

            // add genesis to the known blocks and add the coinbase transaction to known txs
            p->knownBlocks_[genesis.id] = genesis;
            p->knownTxs_.insert(std::make_pair(genesis.cbTx.id, genesis.cbTx));
        }

    }

    void BitcoinPeer::performComputation() {
        /* 
        steps of computation: 
        1. Check for incoming transactions/blocks
            - if transaction found, add to current blockchain top block
            - add to mempool?
            - if block found, broadcast it, and add it to blockchain
            - if fork, uhhhh do something
        2. Try mining current head block
            - if mined, broadcast to all connections
        3. Try making transaction
            - if successful, broadcast transaction
        4. Remove confirmed transactions from mempool
            - not entirely sure how this will go
            - still pretty unfamiliar with why the mempool exists
        */

        // 1. Check for incoming transactions/blocks, adding them to knownBlocks/knownTxs/mempool
        checkInStream();

        // goes through transactions, adds them to the current top block
        if (!mempool_.empty()) {
            auto bkIt = knownBlocks_.find(topBlockID_);
            for (auto& t : mempool_) {
                // check to see if the transaction is not already confirmed
                auto txIt = knownTxs_.find(t.id);
                if (txIt == knownTxs_.end() && topBlockID_ != -1 && bkIt != knownBlocks_.end()) {
                    // insert transaction into block
                    bkIt->second.txs.insert(t);
                    // increment number of transactions in block
                    ++bkIt->second.numTx;
                }
            }
        }

        // 2. Mine head block

        // 3. Try making transaction
        // This will be done by the provided txProbability_

        // 4. Remove confirmed transactions from mempool?
        
    }

    void BitcoinPeer::endOfRound(std::vector<Peer*>& peers) {
        // do end of simulation things
    }

    void BitcoinPeer::checkInStream() {
        while(!inStreamEmpty()) {
            Packet packet = popInStream();
            json msg = packet.getMessage();

            if (msg.contains("type") && msg["type"] == "tx") {
                // incoming transactions need to be processed
                // this includes inserting into knownTxs_ and mempool_
                Tx incomingTx;
                incomingTx.id = msg["contents"]["txID"];
                incomingTx.roundSent = msg["contents"]["roundSent"];
                incomingTx.sender = msg["contents"]["senderID"];
                incomingTx.receiver = msg["contents"]["receiverID"];

                // check to see if the incoming transaction already exists
                auto it = knownTxs_.find(incomingTx.id);
                if (it != knownTxs_.end()) {
                    // insert into knownTxs and mempool
                    knownTxs_.insert(std::make_pair(incomingTx.id, incomingTx));
                    mempool_.insert(incomingTx);

                    // broadcast to all neighbors
                    broadcast(buildTxMessage(incomingTx));
                }
            }
            else if (msg.contains("type") && msg["type"] == "block") {

            }
            else if (msg.contains("type") && msg["type"] == "reqtx") {
                // do later
            }
            else if (msg.contains("type") && msg["type"] == "cmpblock") {
                // do later
            }
            else if (msg.contains("type") && msg["type"] == "reqblock") {
                // do later
            }
            
        }
    }

    void BitcoinPeer::logMinedBlock(const Bk& block) {
        json payload = buildBlockMessage(block);
        LogWriter::pushValue(std::to_string(publicId()), payload);
    }

    json BitcoinPeer::buildTxMessage(const Tx& transaction) {
        json result;
        result["type"] = "tx";
        result["contents"]["txID"] = transaction.id;
        result["contents"]["roundSent"] = transaction.roundSent;
        result["contents"]["senderID"] = transaction.sender;
        result["contents"]["receiverID"] = transaction.receiver;
        return result;
    }

    json BitcoinPeer::buildBlockMessage(const Bk& block) {
        json result;
        result["type"] = "block";
        result["contents"]["blockID"] = block.id;
        result["contents"]["prevID"] = block.prevID;
        result["contents"]["height"] = block.height;
        result["contents"]["roundMined"] = block.roundMined;
        result["contents"]["numberOfTransactions"] = block.numTx;
        // log set of transaction ids (int)
        result["contents"]["transactionIDs"] = json::array();
        for (auto& t : block.txs) {
            result["contents"]["transactionIDs"].push_back(t.id);
        }
        return result;
    }

    
}