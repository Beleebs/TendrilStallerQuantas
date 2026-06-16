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

    // sets the parameters for each node
    void BitcoinPeer::initParameters(const std::vector<Peer*>& peers, json parameters) {
        std::cout << "in initParameters" << std::endl;
        const std::vector<BitcoinPeer*>& bp = reinterpret_cast<const std::vector<BitcoinPeer*>&>(peers); 
        std::cout << "after reinterpret cast" << std::endl;
        // OutputWriter::setLogFile(parameters["outputFile"]);

        // init the genesis block
        Bk genesis;
        genesis.id = -1;
        genesis.height = 0;
        genesis.prevID = -2;
        genesis.roundMined = 1;
        genesis.numTx = 1;
        Tx genesisTx;
        genesisTx.id = -1;
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

            // set the top block ID to the genesis node id
            p->topBlockID_ = genesis.id;

            // add genesis to the known blocks and add the coinbase transaction to known txs
            p->knownBlocks_.insert(std::make_pair(genesis.id, genesis));
            p->knownTxs_.insert(std::make_pair(genesis.cbTx.id, genesis.cbTx));

            // next, setup each node with a new first block
            Bk newCurrentBlock;
            newCurrentBlock.id = p->publicId() * 1000000;
            newCurrentBlock.prevID = p->topBlockID_;
            newCurrentBlock.height = 1;
            // create new coinbase transaction
            Tx newCBTx;
            newCBTx.id = p->publicId() * 1000000 + p->txsMade_;
            newCBTx.sender = -1;
            newCBTx.receiver = p->publicId();
            newCBTx.roundSent = RoundManager::currentRound();
            newCurrentBlock.cbTx = newCBTx;
            newCurrentBlock.txs.insert(newCBTx);
            newCurrentBlock.numTx = newCurrentBlock.txs.size();

            p->currentBlock_ = newCurrentBlock;
            // p->knownBlocks_.insert(std::make_pair(newCurrentBlock.id, newCurrentBlock));
            p->knownTxs_.insert(std::make_pair(newCBTx.id, newCBTx));
            ++p->txsMade_;
            p->mempool_.push_back(newCBTx);
        }
        std::cout << "finished initParameters" << std::endl << std::endl;
        std::cout << std::endl << "(" << RoundManager::currentRound() + 1 << "/" << RoundManager::lastRound() << ")" << std::endl;
    }

    void BitcoinPeer::performComputation() {
        std::cout << "entered performComputation for node: " << publicId() << std::endl;
        // 1. Check for incoming transactions/blocks, adding them to knownBlocks/knownTxs/mempool
        checkInStream();
        std::cout << "completed checkInStream" << std::endl;

        // goes through unconfirmed transactions, adds them to the current top block
        // updates num of transactions
        std::cout << "adding mempool txs to current block" << std::endl << "\t";
        if (!mempool_.empty()) {
            for (const auto& utx : mempool_) {
                std::cout << utx.id << " ";
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
            std::cout << "\tblock mined!! new top height: " << currentBlock_.height << std::endl;
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
            ++blocksMined_;
            knownBlocks_.insert(std::make_pair(currentBlock_.id, currentBlock_));
            std::cout << "\tinserted block id: " << currentBlock_.id << " into knownBlocks_." << std::endl;
            logMinedBlock(currentBlock_);

            // 3. create new top block 
            std::cout << "\tcreating new top block" << std::endl;
            Bk newCurrentBlock;
            // blockID similar to 
            newCurrentBlock.id = publicId() * 1000000 + blocksMined_;
            newCurrentBlock.height = currentBlock_.height + 1;
            newCurrentBlock.prevID = currentBlock_.id;

            // 4. create coinbase transaction for mining the block and insert
            std::cout << "\tcreating coinbase transaction" << std::endl;
            Tx newCBTx;
            newCBTx.id = publicId() * 1000000 + txsMade_;                               // change this
            newCBTx.roundSent = RoundManager::currentRound();
            newCBTx.receiver = publicId();
            ++txsMade_;
            // broadcast new transaction
            // broadcast(buildTxMessage(newCBTx));

            // add coinbase transaction
            newCurrentBlock.txs.insert(newCBTx);
            newCurrentBlock.numTx = newCurrentBlock.txs.size();
            newCurrentBlock.cbTx = newCBTx;

            // update state again
            knownTxs_.insert(std::make_pair(newCBTx.id, newCBTx));
            mempool_.push_back(newCBTx);


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
            newTransaction.sender = (int)publicId();
            std::cout << newTransaction.id << std::endl;

            // find random peer to make transaction to
            std::set<interfaceId> neighborSet = neighbors();
            if (!neighborSet.empty()) {
                int numNeighbors = neighbors().size();
                int index = rand() % numNeighbors;
                auto it = neighborSet.begin();
                std::advance(it, index);
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
            std::cout << p->publicId() << "'s local blockchain: " << p->buildBlockChain() << std::endl;
        }
        

        OutputWriter::pushValue("UTXOsPerRound", totalUTXOs);
        std::cout << std::endl << "(" << RoundManager::currentRound() + 1 << "/" << RoundManager::lastRound() << ")" << std::endl;
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
        std::cout << "END OF EXPERIMENT. PRINTING LOCAL BLOCKCHAINS: " << std::endl;
        for (BitcoinPeer* p : bpeers) {
            std::cout << "\t" << p->publicId() << ": " << p->buildBlockChain() << std::endl;
        }
        std::cout << std::endl;
    }

    void BitcoinPeer::checkInStream() {
        std::cout << "in checkInStream" << std::endl;
        while(!inStreamEmpty()) {
            Packet packet = popInStream();
            json msg = packet.getMessage();

            // transaction issue with mempool currently
            if (msg.contains("type") && msg["type"] == "tx") {
                // incoming transactions need to be processed
                // this includes inserting into knownTxs_ and mempool_
                Tx incomingTx = buildTxFromMessage(msg);

                // check to see if the incoming transaction already exists
                auto it = knownTxs_.find(incomingTx.id);
                if (it == knownTxs_.end()) {
                    std::cout << "\tfound incoming transaction, ID: " << incomingTx.id << std::endl;
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
                // forks are determined by height.

                // flag to determine if block is valid and needs to be switched out
                bool isValidBlock = true;
                Bk incomingBlock = buildBlockFromMessage(msg);
                std::cout << "\tfound incoming block, ID: " << incomingBlock.id << ", height: " << incomingBlock.height << std::endl;
                if (knownBlocks_.find(incomingBlock.id) == knownBlocks_.end()) {
                    knownBlocks_.insert(std::make_pair(incomingBlock.id, incomingBlock));
                    std::cout << "\t\tnot found in knownBlocks_, continue" << std::endl;

                    // greater height, we might need to restructure the local blockchain
                    // due to consensus, if the incoming block's height is higher, then we can switch to that branch.
                    auto topIt = knownBlocks_.find(topBlockID_);
                    // if (incomingBlock.height > currentBlock_.height) {
                    if (incomingBlock.height > knownBlocks_[topBlockID_].height) {
                        // std::cout << "\t\tincomingBlock's height: " << incomingBlock.height << " > currentBlock_'s height: " << currentBlock_.height << ". Switching chain..." << std::endl;
                        std::cout << "\t\tincomingBlock's height: " << incomingBlock.height << " > topBlockID_'s height: " << topIt->second.height << ". Switching chain..." << std::endl;
                        // trace the incoming block's chain for a common branch point
                        int ibPrevID = incomingBlock.prevID;
                        int prevID = currentBlock_.prevID;
                        int commonID = -2;

                        auto ibit = knownBlocks_.find(ibPrevID);
                        auto it = knownBlocks_.find(prevID);

                        // align the two iterators to the same height
                        if (it != knownBlocks_.end() && ibit != knownBlocks_.end()) {
                            while (it->second.height > ibit->second.height) {
                                it = knownBlocks_.find(it->second.prevID);
                            }

                            while (ibit->second.height > it->second.height) {
                                ibit = knownBlocks_.find(ibit->second.prevID);
                            }

                            while (it != ibit) {
                                it = knownBlocks_.find(it->second.prevID);
                                ibit = knownBlocks_.find(ibit->second.prevID);
                                if (it == knownBlocks_.end() || ibit == knownBlocks_.end()) {
                                    break;
                                }
                                else {
                                    std::cout << "\t\t\t\theight: " << it->second.height << ", it id: " << it->second.id << ", ibit id: " << ibit->second.id << std::endl;
                                }
                            }
                        }
                        
                        if (it != knownBlocks_.end() && it == ibit) {
                            std::cout << "\t\tfound common branch: " << it->second.id << std::endl;
                            commonID = it->second.id;
                        }
                        else {
                            isValidBlock = false;
                            std::cout << "\t\tcould not find common branch off of fork." << std::endl;
                        }
                    }
                    // incomingBlock's height <= currentBlock's height
                    else {
                        std::cout << "\t\tincomingBlock's height: " << incomingBlock.height << " <= topBlockID_'s height: " << topIt->second.height << ". Keeping Chain." << std::endl;
                        isValidBlock = false;
                    }

                    // check for incoming block txs, see if they are valid
                    for (const auto &t : incomingBlock.txs) {
                        std::cout << "\t\t\tchecking for transaction: " << t.id << std::endl;
                        // if tx is not found in mempool, but was already verified (attempted double spend), this is an invalid block!!!
                        auto memIt = std::find(mempool_.begin(), mempool_.end(), t);
                        if (memIt == mempool_.end() && knownTxs_.find(t.id) != knownTxs_.end()) {
                            isValidBlock = false;
                            std::cout << "\t\t\t! invalid tx found: " << t.id<< std::endl;
                            break;
                        }
                    }

                    if (isValidBlock) {
                        // set the new topBlockID to the incomingblock
                        topBlockID_ = incomingBlock.id;

                        // remove the incoming block transactions from the mempool
                        for (const auto &t : incomingBlock.txs) {
                            auto memIt = std::find(mempool_.begin(), mempool_.end(), t);
                            if (memIt != mempool_.end()) {
                                mempool_.erase(memIt);
                            }
                        }

                        // next, create new current block to go off of
                        Bk newCurrentBlock;
                        newCurrentBlock.id = currentBlock_.id;
                        newCurrentBlock.prevID = incomingBlock.id;
                        newCurrentBlock.height = incomingBlock.height + 1;
                        Tx newCBTx;
                        newCBTx.id = publicId() * 1000000 + txsMade_;
                        newCBTx.sender = -1;
                        newCBTx.receiver = publicId();
                        newCBTx.roundSent = RoundManager::currentRound();
                        newCurrentBlock.cbTx = newCBTx;
                        newCurrentBlock.txs.insert(newCBTx);
                        newCurrentBlock.numTx = newCurrentBlock.txs.size();

                        // update node state with new current block
                        currentBlock_ = newCurrentBlock;
                        // knownBlocks_.insert(std::make_pair(newCurrentBlock.id, newCurrentBlock));
                        knownTxs_.insert(std::make_pair(newCBTx.id, newCBTx));
                        ++txsMade_;
                        mempool_.push_back(newCBTx);
                        std::cout << "\tcurrentBlock_ = " << currentBlock_.id << ", built from topBlockID_: " << topBlockID_ << std::endl;

                        // broadcast
                        broadcast(buildBlockMessage(incomingBlock));
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
        result["contents"]["ID_sender"] = transaction.sender;
        result["contents"]["ID_receiver"] = transaction.receiver;
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
        result.sender = msg["contents"]["ID_sender"];
        result.receiver = msg["contents"]["ID_receiver"];
        return result;
    }

    std::string BitcoinPeer::buildBlockChain() {
        std::string bcResult;
        int currentID = topBlockID_;
        int nextID;
        bcResult = "tip";
        auto foundBlock = knownBlocks_.find(topBlockID_);
        while (foundBlock != knownBlocks_.end()) {
            currentID = foundBlock->second.id;
            bcResult = std::to_string(currentID) + ", " + bcResult;
            nextID = foundBlock->second.prevID;
            if (currentID == nextID) {
                break;
            }
            else {
                foundBlock = knownBlocks_.find(nextID);
            }
        }
        return bcResult;
    }
    
}