// Ben Leber
// 5/20/2026
// Implementation based off of the v0.10.0 version of Bitcoin 

#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <algorithm>

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
        // 1. Check for incoming transactions/blocks, adding them to knownBlocks/knownTxs/mempool
        checkInStream();

        // goes through unconfirmed transactions, adds them to the current top block
        // updates num of transactions
        if (!mempool_.empty()) {
            for (const auto& utx : mempool_) {
                currentBlock_.txs.insert(utx);
            }
            currentBlock_.numTx = currentBlock_.txs.size();
        }

        // 2. Try mining current block
        int blockResult = (rand() % 100) + 1;
        if (blockResult >= 100 - (100 * mineProbability_)) {
            // when block is successfully mined:
            // 1. broadcast block to other nodes
            // 2. update miner state
            // 3. create new currentBlock_ (initialize with new ids and such)
            // 4. create transaction for mining the block
            // 5. update currentBlock_

            // 1. block successfully mined, broadcast
            // firstly, set the round the block was mined
            currentBlock_.roundMined = RoundManager::currentRound();
            broadcast(buildBlockMessage(currentBlock_));

            // 2. update state
            topBlockID_ = currentBlock_.id;
            // erase txs from mempool that were in that block
            for (const auto& ctx : currentBlock_.txs) {
                auto memIt = std::find(mempool_.begin(), mempool_.end(), ctx);
                if (memIt != mempool_.end()) {
                    mempool_.erase(memIt);
                }
            }

            // 3. create new top block 
            Bk newCurrentBlock;
            // blockID similar to 
            newCurrentBlock.id = publicId() * 1000000 + blocksMined_;
            newCurrentBlock.height = currentBlock_.height + 1;
            newCurrentBlock.prevID = currentBlock_.height;

            // 4. create coinbase transaction for mining the block and insert
            Tx newCBTx;
            newCBTx.id = publicId() * 1000000 + txsMade_;
            newCBTx.roundSent = RoundManager::currentRound();
            newCBTx.receiver = publicId();
            // broadcast new transaction
            broadcast(buildTxMessage(newCBTx));

            // add coinbase transaction
            newCurrentBlock.txs.insert(newCBTx);
            newCurrentBlock.numTx = newCurrentBlock.txs.size();
            newCurrentBlock.cbTx = newCBTx;

            // 5. update currentBlock_
            currentBlock_ = newCurrentBlock;
        }

        // 3. Try making transaction
        // This will be done by the provided txProbability_
        int txResult = (rand() % 100) + 1;
        if (txResult >= 100 - (100 * txProbability_)) {
            // make transaction with random connected peer
            Tx newTransaction;
            // transaction ID is calculated based on the amount of transactions the current block has made
            // has a constraint of ~1000000 possible transactions per node (can probably be done better)
            newTransaction.id = publicId() * 1000000 + txsMade_;
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
            mempool_.push_back(newTransaction);

            // broadcast the new transaction to all neighbors
            broadcast(buildTxMessage(newTransaction));
            ++txsMade_;
        }
    }

    void BitcoinPeer::endOfRound(std::vector<Peer*>& peers) {
        // do something
        // check knownBlock backlog to see if there are any blocks that arrived out of order?
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
                    mempool_.push_back(incomingTx);

                    // broadcast to all neighbors
                    broadcast(buildTxMessage(incomingTx));
                }
            }
            else if (msg.contains("type") && msg["type"] == "block") {
                // we need to check to see if the block is first valid
                // check prevID, make sure that it matches topBlockID_
                // if not, ignore it

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
        OutputWriter::pushValue(std::to_string(publicId()), payload);
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