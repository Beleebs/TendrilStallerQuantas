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

            // next, setup each node with a new first block

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
        */

        // 1. Check for incoming transactions/blocks, adding them to knownBlocks/knownTxs/mempool
        checkInStream();

        // goes through transactions, adds them to the current top block
        if (!mempool_.empty()) {
            auto bkIt = knownBlocks_.find(topBlockID_);
            for (auto& t : mempool_) {
                // check to see if the transaction is not already confirmed
                if (topBlockID_ != -1 && bkIt != knownBlocks_.end()) {
                    // insert transaction into block
                    bkIt->second.txs.insert(t);
                    // increment number of transactions in block
                    ++bkIt->second.numTx;
                }
            }
        }

        // 2. Mine head block
        int blockResult = (rand() % 100) + 1;
        if (blockResult >= 100 - (100 * mineProbability_)) {
            // block is mined, meaning that it needs to stop inserting transactions
            // any transactions found in the block need to be removed from mempool as well
            auto it = knownBlocks_.find(topBlockID_);
            // also, the block is inserted into known blocks
            // topBlockID_ is set to the id of the current block

        }

        // 3. Try making transaction
        // This will be done by the provided txProbability_
        int txResult = (rand() % 100) + 1;
        if (txResult >= 100 - (100 * txProbability_)) {
            // make transaction with random connected peer
            Tx newTransaction;

            // transaction ID is calculated based off of previous highest trailing ID value found in knownTxs_
            // for example: 20000*05* < 10000*10*
            int highestID = 0;
            for (auto& t : knownTxs_) {
                int result = t.second.id % 1000000;
                if (result > highestID) {
                    highestID = result;
                }
            }
            newTransaction.id = publicId() * 1000000 + highestID + 1;
            newTransaction.roundSent = RoundManager::currentRound();
            newTransaction.sender = publicId();

            // find random peer to make transaction to
            if (!neighbors().empty()) {
                int numNeighbors = neighbors().size();
                int index = rand() % numNeighbors;
                auto it = neighbors().begin();
                for (int i = 0; i < index; ++i) {
                    ++it;
                }
                newTransaction.receiver = *it;
            }
            // add to mempool and knownTxs
            knownTxs_.insert(std::make_pair(newTransaction.id, newTransaction));
            mempool_.insert(newTransaction);

            // broadcast the new transaction to all neighbors
            broadcast(buildTxMessage(newTransaction));
        }
        
    }

    void BitcoinPeer::endOfRound(std::vector<Peer*>& peers) {
        /*
            List of things that need to happen:
            1. receive blocks (if any)
            2. remove confirmed transactions from mempool
        */
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
                // incoming blocks need to be checked for similar id/prev id with topBlockID_
                auto it = knownBlocks_.find(topBlockID_);
                auto it2 = knownBlocks_.find(it->second.prevID);
                if (it2->second.id == msg["contents"]["prevID"]) {
                    int prevBlock = it2->second.id;
                    topBlockID_ = msg["contents"]["blockID"];
                }
                // else, discard block
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