// Ben Leber
// 6/23/2026
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

        Block genesis;
        genesis.id = -1;
        genesis.prevId = -2;
        genesis.roundMined = 0;
        genesis.height = 0;
        genesis.miner = -1;
        genesis.prevMiner = -2;
        Transaction genesisTxn;
        genesisTxn.id = -1;
        genesisTxn.roundCreated = 0;
        genesisTxn.source = -1; 
        genesisTxn.receiver = -2;
        genesis.txns.insert(genesisTxn);

        for (BitcoinPeer* bp : bpeers) {
            // mining probability & txn probability
            bp->mineProbability_ = parameters.value("mineProbability", mineProbability_);
            bp->txnProbability_ = parameters.value("txnProbability", txnProbability_);
            bp->knownBlocks_.insert(genesis);
            bp->tip_ = genesis;

            // setup new top block
            bp->candidate_ = bp->createNewBlock();
            std::string s = "Node " + std::to_string(bp->publicId()) + " setup.";
            QUANTAS_LOG_INFO("ip") << s;
        }
        std::string finalMsg = "All (" + std::to_string(bpeers.size()) + ") nodes ready to go.\n";
        QUANTAS_LOG_INFO("ip") << finalMsg;
    }

    void BitcoinPeer::performComputation() {
        // std::string greetingMessage = "Node " + std::to_string(publicId()) + ": ";
        // int dialogueSelection = rand() % 5;
        // switch(dialogueSelection) {
        //     case 0:
        //         greetingMessage += "Greetings!";
        //         break;
        //     case 1:
        //         greetingMessage += "... hi.";
        //         break;
        //     case 2:
        //         greetingMessage += "yo wsg";
        //         break;
        //     case 3:
        //         greetingMessage += "#fairs";
        //         break;
        //     case 4:
        //         greetingMessage += "HI!!!!!!!!";
        //         break;
        //     default:
        //         greetingMessage += "hello.";
        // }
        // QUANTAS_LOG_DEBUG("pc") << greetingMessage;

        // // check for incoming messages
        checkInStream();

        // // attempt to mine the head block
        attemptMine();

        // // attempt to introduce a new transaction
        attemptTxn();
    }

    void BitcoinPeer::endOfRound(std::vector<Peer*>& peers) {
        std::vector<BitcoinPeer*> bpeers = reinterpret_cast<std::vector<BitcoinPeer*>&>(peers);
        int txsMade = 0;
        int msgsSent = 0;
        for (BitcoinPeer* bp : bpeers) {
            txsMade += bp->txnsMadeThisRound_;
            msgsSent += bp->msgsSentThisRound_;
            bp->txnsMadeThisRound_ = 0;
            bp->msgsSentThisRound_ = 0;
        }

        QUANTAS_LOG_INFO("eor") << "End of Round " + std::to_string(RoundManager::currentRound() - 1) + ". Transactions made: " + std::to_string(txsMade) + ", Messages sent: " + std::to_string(msgsSent) << std::endl;
        // count blocks mined later
    }

    void BitcoinPeer::endOfExperiment(std::vector<Peer*>& peers) {
        std::vector<BitcoinPeer*> bpeers = reinterpret_cast<std::vector<BitcoinPeer*>&>(peers);
    }

    Block BitcoinPeer::createNewBlock() {
        Block b;
        b.id = blocksMined_;
        b.prevId = tip_.id;
        b.prevMiner = tip_.miner;
        b.roundMined = -1;
        b.miner = publicId();
        b.height = tip_.height + 1;
        // insert the new coinbase transaction
        b.txns.insert(createNewTransaction(-1, publicId()));
        ++txnsMadeThisRound_;   // debug info
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
            // debug
            std::string debugOutput = 
                "Block mined, ID: " + std::to_string(candidate_.id) + 
                ", Miner ID: " + std::to_string(candidate_.miner) + 
                ", Height: " + std::to_string(candidate_.height);
            QUANTAS_LOG_DEBUG("attemptMine") << debugOutput;

            // insert correct block transactions
            insertValidTransactions(candidate_);
            QUANTAS_LOG_TRACE("attemptMine") << "After insertValidTransactions()";

            // block has been mined
            candidate_.roundMined = RoundManager::currentRound();
            knownBlocks_.insert(candidate_);
            tip_ = candidate_;
            ++blocksMined_;
            Block newCandidate = createNewBlock();
            QUANTAS_LOG_TRACE("attemptMine") << "After creating new block";

            // build header message
            broadcast(buildHeaderMsg(candidate_));
            QUANTAS_LOG_TRACE("attemptMine") << "After header broadcast";

            // finally update state and continue
            candidate_ = newCandidate;
            QUANTAS_LOG_TRACE("attemptMine") << "After updating new candidate";
        }
    }

    void BitcoinPeer::insertValidTransactions(Block& b) {
        // parse current blockchain
        std::set<Transaction> foundTxns;
        Block current = tip_;
        QUANTAS_LOG_TRACE("insertValidTransactions") << "Before parsing blockchain";
        while (current.prevId != -2 || current.prevMiner != -2) {
            QUANTAS_LOG_TRACE("insertValidTransactions") << "current block id: " << current.id << ", miner id: " << current.miner;
            Block prevBlock = getStoredBlock(current.prevId, current.prevMiner);
            for (const auto& t : current.txns) {
                foundTxns.insert(t);
            }
            current = prevBlock;
        }

        QUANTAS_LOG_TRACE("insertValidTransactions") << "Before inserting txns";
        // until the current block is full, insert transactions that aren't found in foundTxns
        for (const auto& t : mempool_) {
            if (!b.isFull()) {
                auto it = foundTxns.find(t);
                if (it == foundTxns.end()) {
                    b.txns.insert(t);
                    QUANTAS_LOG_TRACE("insertValidTransactions") << "inserting txn id: " << t.id << " from source id: " << t.source << ". New block size: " << b.txns.size();
                }
            }
            else {
                QUANTAS_LOG_TRACE("insertValidTransactions") << "block full, exiting...";
                break;
            } 
        }

        QUANTAS_LOG_TRACE("insertValidTransactions") << "end of insertValidTransactions";
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
                mempool_.push_back(newTx);
                ++txnsMade_; ++txnsMadeThisRound_;
            }
            else {
                newTx = createNewTransaction(publicId(), -1);
                mempool_.push_back(newTx);
                ++txnsMade_; ++txnsMadeThisRound_;
            }
            json newTxMsg = buildTxnMsg(newTx);
            std::string debugOutput = "Transaction Made, Source: " + std::to_string(newTx.source) + ", ID: " + std::to_string(newTx.id) + ", Recipient: " + std::to_string(newTx.receiver);
            QUANTAS_LOG_DEBUG("attemptTxn") << debugOutput;
            // broadcast(newTxMsg);     // standard output
            randomMulticast(newTxMsg);  // used for creating GET_BLOCK_TXN requests
            ++msgsSentThisRound_;
        }
    }

    bool BitcoinPeer::hasBlock(const Block& b) const {
        auto it = knownBlocks_.find(b);
        if (it != knownBlocks_.end()) {
            return true;
        }
        else {
            return false;
        }
    }

    bool BitcoinPeer::hasBlock(const int& id, const interfaceId& minerId) const {
        Block tempBlock;
        tempBlock.id = id;
        tempBlock.miner = minerId;
        auto it = knownBlocks_.find(tempBlock);
        if (it != knownBlocks_.end()) {
            return true;
        }
        else {
            return false;
        }
    }

    bool BitcoinPeer::hasTxn(const Transaction& t) const {
        auto it = std::find(mempool_.begin(), mempool_.end(), t);
        if (it != mempool_.end()) {
            return true;
        }
        else {
            return false;
        }
    }

    bool BitcoinPeer::hasTxn(const int& id, const interfaceId& sourceId) const {
        Transaction tempTxn;
        tempTxn.id = id;
        tempTxn.source = sourceId;
        auto it = std::find(mempool_.begin(), mempool_.end(), tempTxn);
        if (it != mempool_.end()) {
            return true;
        }
        else {
            return false;
        }
    }

    Block BitcoinPeer::getStoredBlock(const int& id, const interfaceId& minerId) const {
        Block tempBlock;
        tempBlock.id = id;
        tempBlock.miner = minerId;
        auto it = knownBlocks_.find(tempBlock);
        if (it != knownBlocks_.end()) {
            return *it;
        }
        else {
            Block empty;
            return empty;
        }
    }

    Transaction BitcoinPeer::getStoredTxn(const int& id, const interfaceId& sourceId) const {
        Transaction tempTxn;
        tempTxn.id = id;
        tempTxn.source = sourceId;
        auto it = std::find(mempool_.begin(), mempool_.end(), tempTxn);
        if (it != mempool_.end()) {
            return *it;
        }
        else {
            Transaction empty;
            return empty;
        }
    }

    // TODO: implement message sending between nodes w this
    void BitcoinPeer::checkInStream() {
        while (!inStreamEmpty()) {
            Packet p = popInStream();
            json msg = p.getMessage();
            std::string debugOutput;
            // deal with specifics based on the message type
            if (msg["type"] == MessageType::HEADER && !isWaiting_) {
                // debug
                debugOutput = 
                    "Node " + std::to_string(publicId()) + 
                    " recieved HEADER from " + std::to_string(p.sourceId()) + 
                    ". (delay: " + std::to_string(p.getDelay()) + ")";
                QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                int blockId = msg["content"]["block_id"];
                interfaceId minerId = msg["content"]["miner_id"];
                if (!hasBlock(blockId, minerId)) {
                    // debug
                    debugOutput = 
                    "New block, block ID: " + std::to_string(blockId) +
                    ", miner ID: " + std::to_string(minerId);
                    QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    Block tempBlock;
                    tempBlock.id = blockId;
                    tempBlock.miner = minerId;
                    unicastTo(buildGetDataMsg(tempBlock), p.sourceId());
                    ++msgsSentThisRound_;
                    isWaiting_ = true;
                }
                // else just ignore the header
                else {
                    // debug
                    debugOutput = "Block already exists.";
                    QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                    continue;
                }
            }
            else if (msg["type"] == MessageType::GET_DATA) {
                // debug
                debugOutput = 
                    "Node " + std::to_string(publicId()) + 
                    " recieved GET_DATA from " + std::to_string(p.sourceId()) + 
                    ". (delay: " + std::to_string(p.getDelay()) + ")";
                QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                int blockId = msg["content"]["block_id_requested"];
                interfaceId minerId = msg["content"]["miner_id"];
                if (hasBlock(blockId, minerId)) {
                    // debug
                    debugOutput = 
                        "Block found, sending CMP_BLOCK for Block ID: " + std::to_string(blockId) +
                        " to recipient: " + std::to_string(p.sourceId());
                    QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    Block tempBlock = getStoredBlock(blockId, minerId);
                    unicastTo(buildCmpBlockMsg(tempBlock), p.sourceId());
                    ++msgsSentThisRound_;
                }
                else {
                    Block tempBlock;
                    unicastTo(buildCmpBlockMsg(tempBlock), p.sourceId());
                    ++msgsSentThisRound_;
                }
                
            }
            else if (msg["type"] == MessageType::CMP_BLOCK) {
                int blockId = msg["content"]["block_id"];
                interfaceId minerId = msg["content"]["miner_id"];
                auto it = hbnNeighbors_.find(p.sourceId());
                Block newBlock;

                // if the CMP_BLOCK sent was from one of the HBN neighbors
                if (it != hbnNeighbors_.end() && !isWaiting_) {
                    // debug
                    debugOutput = 
                        "Node " + std::to_string(publicId()) + 
                        " recieved CMP_BLOCK from HBN neighbor " + std::to_string(p.sourceId()) + 
                        ". (delay: " + std::to_string(p.getDelay()) + ")";
                    QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    // need to build block off of it
                    newBlock = buildBlockFromMsg(msg);
                }
                // if the CMP_BLOCK was sent in response to a GET_DATA
                else if (it == hbnNeighbors_.end() && isWaiting_) {
                    if (blockId != -2) {
                        // debug
                        debugOutput = 
                            "Node " + std::to_string(publicId()) + 
                            " recieved CMP_BLOCK from Non-HBN neighbor " + std::to_string(p.sourceId()) + 
                            ". (delay: " + std::to_string(p.getDelay()) + ")";
                        QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                        isWaiting_ = false;
                        newBlock = buildBlockFromMsg(msg);
                    }
                    else {
                        // debug
                        debugOutput = 
                            "Empty block sent, sender ID " + std::to_string(p.sourceId()) +
                            " does not have requested block.";
                        QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                    }
                }
                // else, drop it. (unwanted unsolicited CMP_BLOCK)
                else {
                    // debug
                    debugOutput = "Received unwanted unsolicited CMP_BLOCK from: " + std::to_string(p.sourceId());
                    QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                    continue;
                }

                // if the node does not have all of the transactions listed in the block
                if (!hasAllCmpBlockTxns(msg)) {
                    // debug
                    debugOutput = 
                        "Node " + std::to_string(publicId()) + 
                        " doesn't have all txns found in block id: " + std::to_string(blockId) + 
                        ", from miner id: " + std::to_string(minerId) + 
                        ", sending GET_BLOCK_TXN.";
                    QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    // insert the current block
                    knownBlocks_.insert(newBlock);
                    // need to send GET_BLOCK_TXN request
                    unicastTo(buildGetBlockTxnMsg(newBlock), p.sourceId());
                    ++msgsSentThisRound_;
                    isWaiting_ = true;      // waiting for BlockTxns Message
                }
                else {
                    // debug
                    debugOutput = 
                        "Node " + std::to_string(publicId()) + 
                        " Has all txns found in block id: " + std::to_string(blockId) + 
                        ", from miner id: " + std::to_string(minerId);
                    QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    for (const auto& t : msg["content"]["tx_ids"]) {
                        Transaction tempTxn = getStoredTxn(t["tx_id"], t["source_id"]);
                        if (!newBlock.isFull()) {
                            newBlock.txns.insert(tempTxn);
                        }
                    }
                    // insert new block, set as tip, and create new candidate
                    knownBlocks_.insert(newBlock);

                    // If the new height is greater than the current tip's height, adopt new longest chain
                    if (tip_.height < newBlock.height) {
                        // debug
                        debugOutput = 
                            "newBlock's height: " + std::to_string(newBlock.height) + 
                            " > tip_'s height: " + std::to_string(tip_.height) +
                            ". Setting as new tip and creating new candidate...";
                        QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                        tip_ = newBlock;
                        candidate_ = createNewBlock();

                        // debug
                        debugOutput =
                            "Tip updated! New tip id: " + std::to_string(tip_.id) + 
                            ", miner id: " + std::to_string(tip_.miner) +
                            ". New height: " + std::to_string(tip_.height);
                        QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                    }
                    else {
                        // debug
                        debugOutput = 
                            "newBlock's height: " + std::to_string(newBlock.height) + 
                            " <= tip_'s height: " + std::to_string(tip_.height) +
                            ". Keeping candidate.";
                        QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                    }
                }

            }
            else if (msg["type"] == MessageType::GET_BLOCK_TXN) {
                // debug
                debugOutput = 
                    "Node " + std::to_string(publicId()) + 
                    " recieved GET_BLOCK_TXN from " + std::to_string(p.sourceId()) + 
                    ". (delay: " + std::to_string(p.getDelay()) + ")";
                QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                int blockId = msg["content"]["block_txn_requested"];
                interfaceId minerId = msg["content"]["miner_id"];
                if (hasBlock(blockId, minerId)) {
                    // debug
                    debugOutput = 
                        "Sending txns from block id: " + std::to_string(blockId) + 
                        ", from miner id: " + std::to_string(minerId);
                    QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    Block tempBlock = getStoredBlock(blockId, minerId);
                    unicastTo(buildBlockTxnMsg(tempBlock), p.sourceId());
                    ++msgsSentThisRound_;
                }
            }
            else if (msg["type"] == MessageType::BLOCK_TXN) {
                if (isWaiting_) {
                    // debug
                    debugOutput = 
                        "Node " + std::to_string(publicId()) + 
                        " recieved BLOCK_TXN from " + std::to_string(p.sourceId()) + 
                        ". (delay: " + std::to_string(p.getDelay()) + ")";
                    QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    // pull block from known blocks
                    int blockId = msg["content"]["block_id"];
                    interfaceId minerId = msg["content"]["miner_id"];
                    if (hasBlock(blockId, minerId)) {
                        // debug
                        debugOutput = "Block found, inserting new transactions...";
                        QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                        Block newBlock = getStoredBlock(blockId, minerId);
                        for (const auto& t : getBlockTxnsFromMessage(msg)) {
                            if (!newBlock.isFull()) {
                                newBlock.txns.insert(t);
                                mempool_.push_back(t);      // also insert into the mempool
                            }
                        }
                        // replace old block with updated transactions
                        knownBlocks_.erase(newBlock);
                        knownBlocks_.insert(newBlock);

                        // Same fork logic
                        if (tip_.height < newBlock.height) {
                            // debug
                            debugOutput = 
                                "newBlock's height: " + std::to_string(newBlock.height) + 
                                " > tip_'s height: " + std::to_string(tip_.height) +
                                ". Setting as new tip and creating new candidate...";
                            QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                            tip_ = newBlock;
                            candidate_ = createNewBlock();

                            // debug
                            debugOutput =
                                "Tip updated! New tip id: " + std::to_string(tip_.id) + 
                                ", miner id: " + std::to_string(tip_.miner) +
                                ". New height: " + std::to_string(tip_.height);
                            QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                        }
                        else {
                            // debug
                            debugOutput = 
                                "newBlock's height: " + std::to_string(newBlock.height) + 
                                " <= tip_'s height: " + std::to_string(tip_.height) +
                                ". Keeping candidate.";
                            QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                        }
                        isWaiting_ = false;
                    }
                }
                // else do nothing!! weird propagation
                else {
                    // debug
                    debugOutput = 
                        "Node " + std::to_string(publicId()) + 
                        " recieved random BLOCK_TXN from " + std::to_string(p.sourceId()) + ".";
                    QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                }
            }
            else if (msg["type"] == MessageType::TXN) {
                Transaction incomingTxn = buildTxnFromMsg(msg);
                mempool_.push_back(incomingTxn);
                std::string debugOutput = 
                    "Node " + std::to_string(publicId()) + 
                    " recieved TXN ID: " + std::to_string(incomingTxn.id) + 
                    " from " + std::to_string(p.sourceId()) + 
                    ". (delay: " + std::to_string(p.getDelay()) + ")";
                QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
            }
        }
    }

    // requires: block_id, miner_id
    json BitcoinPeer::buildHeaderMsg(const Block& b) const {
        json msg;    
        msg["type"] = MessageType::HEADER;
        msg["content"]["block_id"] = b.id;
        msg["content"]["miner_id"] = b.miner;
        return msg;
    }

    // requires: block_id_requested, miner_id
    json BitcoinPeer::buildGetDataMsg(const Block& b) const {
        json msg;
        msg["type"] = MessageType::GET_DATA;
        msg["content"]["block_id_requested"] = b.id;
        msg["content"]["miner_id"] = b.miner;
        return msg;
    }

    // requires: block_id, prev_id, round_mined, miner_id, prev_miner_id, height, tx_ids
    json BitcoinPeer::buildCmpBlockMsg(const Block& b) const {
        json msg;
        msg["type"] = MessageType::CMP_BLOCK;
        msg["content"]["block_id"] = b.id;
        msg["content"]["prev_id"] = b.prevId;
        msg["content"]["round_mined"] = b.roundMined;
        msg["content"]["miner_id"] = b.miner;
        msg["content"]["prev_miner_id"] = b.miner;
        msg["content"]["height"] = b.height;
        for (const auto& t : b.txns) {
            json combinedId = {
                {"source_id", t.source},
                {"tx_id", t.id}
            };
            msg["content"]["tx_ids"] += combinedId;
        }
        return msg;
    }

    // requires: block_tx_requested, miner_id
    json BitcoinPeer::buildGetBlockTxnMsg(const Block& b) const {
        json msg;
        msg["type"] = MessageType::GET_BLOCK_TXN;
        msg["content"]["block_txn_requested"] = b.id;
        msg["content"]["miner_id"] = b.miner;
        return msg;
    }

    // requires: block_id, miner_id, txs (bunch of TXN messages)
    json BitcoinPeer::buildBlockTxnMsg(const Block& b) const {
        json msg;
        msg["type"] = MessageType::BLOCK_TXN;
        msg["content"]["block_id"] = b.id;
        msg["content"]["miner_id"] = b.miner;
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
        // only contains the header info
        if (msg["type"] == MessageType::CMP_BLOCK) {
            result.id = msg["content"]["block_id"];
            result.prevId = msg["content"]["prev_id"];
            result.roundMined = msg["content"]["round_mined"];
            result.miner = msg["content"]["miner_id"];
            result.height = msg["content"]["height"];
        }
        // contains txn info
        else if (msg["type"] == MessageType::BLOCK_TXN) {
            int blockId = msg["content"]["block_id"];
            interfaceId minerId = msg["content"]["miner_id"];
            if (hasBlock(blockId, minerId)) {
                result = getStoredBlock(blockId, minerId);
                result.txns = getBlockTxnsFromMessage(msg);
            }
        }
        return result;
    }

    Transaction BitcoinPeer::buildTxnFromMsg(const json& msg) const {
        Transaction result;
        result.id = msg["content"]["tx_id"];
        result.roundCreated = msg["content"]["round_created"];
        result.source = msg["content"]["source_id"];
        result.receiver= msg["content"]["receiver_id"];
        return result;
    }

    // input message is type BLOCK_TXN
    std::set<Transaction> BitcoinPeer::getBlockTxnsFromMessage(const json& msg) const {
        std::set<Transaction> txnSet;
        for (const auto& t : msg["content"]["txs"]) {
            Transaction txn = buildTxnFromMsg(t);
            txnSet.insert(txn);
        }
        return txnSet;
    }

    // incoming message is type CMP_BLOCK
    bool BitcoinPeer::hasAllCmpBlockTxns(const json& msg) const {
        QUANTAS_LOG_TRACE("hasAllCmpBlockTxns") << "In hasAllCmpBlockTxns for Node: " << publicId();
        for (const auto& t : msg["content"]["tx_ids"]) {
            QUANTAS_LOG_DEBUG("hasAllCmpBlockTxns") << "Source ID: " << t["source_id"] << ", Txn ID: " << t["tx_id"];

            // coinbase TXN, ignore
            if (t["source_id"] == -1) {
                QUANTAS_LOG_DEBUG("hasAllCmpBlockTxns") << "\t\tFound CoinBase Txn. Continuing...";
                continue;
            }

            if (!hasTxn(t["tx_id"], t["source_id"])) {
                QUANTAS_LOG_DEBUG("hasAllCmpBlockTxns") << "Returned False, missing transaction.";
                return false;
            }
        }
        QUANTAS_LOG_DEBUG("hasAllCmpBlockTxns") << "Returned True!";
        return true;
    }

}