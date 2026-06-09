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
        std::cout << "in initParameters" << std::endl;
        const std::vector<BitcoinPeer*>& bp = reinterpret_cast<const std::vector<BitcoinPeer*>&>(peers); 
        std::cout << "after reinterpret cast" << std::endl;
        // OutputWriter::setLogFile(parameters["outputFile"]);

        // init the genesis block
        Bk genesis;
        genesis.id = 1;
        genesis.height = 0;
        genesis.prevID = 0;
        genesis.roundMined = 1;
        genesis.numTx = 1;
        Tx genesisTx;
        genesisTx.id = 1;
        genesisTx.roundSent = 1;
        genesis.txs.insert(genesisTx);
        genesis.cbTx = genesisTx;
        std::cout << "created genesis" << std::endl;

        // setup each node
        for (auto p : bp) {
            std::cout << "setting up node: " << p->publicId() << std::endl;
            // change the probability values
            p->mineProbability_ = parameters.value("mineProbability", mineProbability_);
            p->txProbability_ = parameters.value("txProbability", txProbability_);

            // set the top block ID to the genesis node
            p->topBlockID_ = 1;

            // add genesis to the known blocks and add the coinbase transaction to known txs
            p->knownBlocks_.insert(std::make_pair(genesis.id, genesis));
            p->knownTxs_.insert(std::make_pair(genesis.cbTx.id, genesis.cbTx));

            // next, setup each node with a new first block
            Bk newCurrentBlock;
            newCurrentBlock.id = p->publicId() * 1000000 + 1;
            newCurrentBlock.prevID = p->topBlockID_;
            newCurrentBlock.height = 1;
            p->currentBlock_ = newCurrentBlock;
        }
        std::cout << "finished initParameters" << std::endl << std::endl;
    }

    void BitcoinPeer::performComputation() {
        std::cout << "entered performComputation for node: " << publicId() << std::endl;
        // 1. Check for incoming transactions/blocks, adding them to knownBlocks/knownTxs/mempool
        checkInStream();
        std::cout << "completed checkInStream" << std::endl;

        // goes through unconfirmed transactions, adds them to the current top block
        // updates num of transactions
        std::cout << "adding mempool txs to current block: " << currentBlock_.id << std::endl;
        if (!mempool_.empty()) {
            for (const auto& utx : mempool_) {
                std::cout << utx.id;
                currentBlock_.txs.insert(utx);
            }
            currentBlock_.numTx = currentBlock_.txs.size();
            std::cout << std::endl;
        }

        // 2. Try mining current block
        std::cout << "trying to mine block: " << currentBlock_.id << std::endl;
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
            std::cout << "block mined!!" << std::endl;
            currentBlock_.roundMined = RoundManager::currentRound();
            broadcast(buildBlockMessage(currentBlock_));

            // 2. update state
            std::cout << "\tupdating state" << std::endl;
            topBlockID_ = currentBlock_.id;
            // erase txs from mempool that were in that block
            for (const auto& ctx : currentBlock_.txs) {
                auto memIt = std::find(mempool_.begin(), mempool_.end(), ctx);
                if (memIt != mempool_.end()) {
                    mempool_.erase(memIt);
                }
            }

            // 3. create new top block 
            std::cout << "\tcreating new top block" << std::endl;
            Bk newCurrentBlock;
            // blockID similar to 
            newCurrentBlock.id = publicId() * 1000000 + blocksMined_;
            newCurrentBlock.height = currentBlock_.height + 1;
            newCurrentBlock.prevID = currentBlock_.height;

            // 4. create coinbase transaction for mining the block and insert
            std::cout << "\tcreating coinbase transaction" << std::endl;
            Tx newCBTx;
            newCBTx.id = publicId() * 1000000 + txsMade_;                               // change this
            newCBTx.roundSent = RoundManager::currentRound();
            newCBTx.receiver = publicId();
            // broadcast new transaction
            broadcast(buildTxMessage(newCBTx));

            // add coinbase transaction
            newCurrentBlock.txs.insert(newCBTx);
            newCurrentBlock.numTx = newCurrentBlock.txs.size();
            newCurrentBlock.cbTx = newCBTx;

            // 5. update currentBlock_
            std::cout << "\tfinishing block mining" << std::endl;
            currentBlock_ = newCurrentBlock;
        }

        // 3. Try making transaction
        // This will be done by the provided txProbability_
        std::cout << "attempting transaction" << std::endl;
        int txResult = (rand() % 100) + 1;
        if (txResult >= 100 - (100 * txProbability_)) {
            std::cout << "created new transaction: ";
            // make transaction with random connected peer
            Tx newTransaction;
            // transaction ID is calculated based on the amount of transactions the current block has made
            // has a constraint of ~1000000 possible transactions per node (can probably be done better)
            newTransaction.id = publicId() * 1000000 + txsMade_;
            newTransaction.roundSent = RoundManager::currentRound();
            newTransaction.sender = publicId();
            std::cout << newTransaction.id << std::endl;
            for (int i = 0; i < neighbors().size(); ++i) {
                std::cout << i << " ";
            }
            std::cout << std::endl;

            // find random peer to make transaction to
            std::cout << "\tselecting random neighbor: ";

            std::cout << "ok lets do this" << std::endl;

            if (!neighbors().empty()) {
                std::cout << "\tinto the if statement" << std::endl;
                int numNeighbors = neighbors().size();
                int index = rand() % numNeighbors;
                std::cout << "\tfound index: " << index << std::endl;
                auto it = neighbors().begin();
                for (int i = 0; i < index; ++i) {
                    std::cout << "\tbefore dereferencing an iterator" << std::endl;
                    ++it;
                    std::cout << "\tafter: " << *it << std::endl;
                }
                std::cout << "\tdecided receiver: ";
                newTransaction.receiver = *it;
                std::cout << newTransaction.receiver << std::endl;
            }
            // add to mempool and knownTxs
            std::cout << "\tadding to mempool/knownTxs" << std::endl;
            knownTxs_.insert(std::make_pair(newTransaction.id, newTransaction));
            mempool_.push_back(newTransaction);

            // broadcast the new transaction to all neighbors
            std::cout << "\tbroadcasting transaction" << std::endl;
            broadcast(buildTxMessage(newTransaction));
            ++txsMade_;
        }
        std::cout << "finished performComputation" << std::endl << std::endl;
    }

    void BitcoinPeer::endOfRound(std::vector<Peer*>& peers) {
        // do something
        // check knownBlock backlog to see if there are any blocks that arrived out of order?
        int totalUTXOs = 0;
        const std::vector<BitcoinPeer*>& bpeers = reinterpret_cast<const std::vector<BitcoinPeer*>&>(peers);
        for (BitcoinPeer* p: bpeers) {
            totalUTXOs += p->mempool_.size();
        }
        OutputWriter::pushValue("UTXOsPerRound", totalUTXOs);
    }

    void BitcoinPeer::endOfExperiment(std::vector<Peer*>& peers) {
        int totalMinedBlocks = 0;
        int totalTxsMade = 0;
        const std::vector<BitcoinPeer*>& bpeers = reinterpret_cast<const std::vector<BitcoinPeer*>&>(peers);
        for (BitcoinPeer* p : bpeers) {
            totalMinedBlocks += p->blocksMined_;
            totalTxsMade += p->txsMade_;
        }
        OutputWriter::pushValue("totalMinedBlocks", totalMinedBlocks);
        OutputWriter::pushValue("totalTransactions", totalTxsMade);
    }

    void BitcoinPeer::checkInStream() {
        std::cout << "in checkInStream" << std::endl;
        while(!inStreamEmpty()) {
            Packet packet = popInStream();
            json msg = packet.getMessage();

            if (msg.contains("type") && msg["type"] == "tx") {
                // incoming transactions need to be processed
                // this includes inserting into knownTxs_ and mempool_
                Tx incomingTx = buildTxFromMessage(msg);

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
                // then, will need to verify all transactions are within mempool
                // if they aren't in mempool, we would need to make sure that it hasn't already been spent

                // TODO: Implement fork logic

                // flag to determine if block is valid and needs to be switched out
                bool isValidBlock = true;

                if (msg["contents"]["prevID"] == topBlockID_) {
                    for (const auto& id : msg["contents"]["transactions"]) {
                        Tx t;
                        t.id = id.get<int>();
                        // if tx is not found in mempool, but was already verified (attempted double spend)
                        // this is an invalid block!!!
                        if (std::find(mempool_.begin(), mempool_.end(), t) == mempool_.end() && knownTxs_.find(t.id) != knownTxs_.end()) {
                            isValidBlock = false;
                        }
                    }

                    // if valid, we need to first remove all correlating txs from mempool
                    if (isValidBlock) {
                        for (const auto& id : msg["contents"]["transactions"]) {
                            Tx ctx;
                            ctx.id = id.get<int>();
                            auto memIt = std::find(mempool_.begin(), mempool_.end(), ctx);
                            if (memIt != mempool_.end()) {
                                mempool_.erase(memIt);
                            }
                        }

                        // create physical block from message
                        Bk incomingBlock = buildBlockFromMessage(msg);
                        knownBlocks_.insert(std::make_pair(incomingBlock.id, incomingBlock));
                        topBlockID_ = incomingBlock.id;
                    }
                }
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
        result["contents"]["coinbaseTransaction"] = buildTxMessage(block.cbTx);
        result["contents"]["numberOfTransactions"] = block.numTx;
        // log set of transactions (int)
        result["contents"]["transactions"] = json::array();
        for (const auto& t : block.txs) {
            result["contents"]["transactions"].push_back(buildTxMessage(t));
        }
        return result;
    }

    BitcoinPeer::Bk BitcoinPeer::buildBlockFromMessage(const json& msg) {
        Bk result;
        result.id = msg["contents"]["blockID"];
        result.prevID = msg["contents"]["prevID"];
        result.height = msg["contents"]["height"];
        result.roundMined = msg["contents"]["roundMined"];
        result.numTx = msg["contents"]["numberOfTransactions"];
        for (const auto& t : msg["contents"]["transactions"]) {
            Tx incomingTx = buildTxFromMessage(t);
            result.txs.insert(incomingTx);
        }
        result.cbTx = buildTxFromMessage(msg["contents"]["coinbaseTransaction"]);
        return result;
    }

    BitcoinPeer::Tx BitcoinPeer::buildTxFromMessage(const json& msg) {
        Tx result;
        result.id = msg["contents"]["txID"];
        result.roundSent = msg["contents"]["roundSent"];
        result.sender = msg["contents"]["senderID"];
        result.receiver = msg["contents"]["receiverID"];
        return result;
    }
    
}