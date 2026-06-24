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
        std::string greetingMessage = "Node " + std::to_string(publicId()) + ": ";
        int dialogueSelection = rand() % 5;
        switch(dialogueSelection) {
            case 0:
                greetingMessage += "Greetings!";
                break;
            case 1:
                greetingMessage += "... hi.";
                break;
            case 2:
                greetingMessage += "yo wsg";
                break;
            case 3:
                greetingMessage += "#fairs";
                break;
            case 4:
                greetingMessage += "HI!!!!!!!!";
                break;
            default:
                greetingMessage += "hello.";
        }
        QUANTAS_LOG_DEBUG("pc") << greetingMessage;

        // // check for incoming messages
        checkInStream();

        // // attempt to mine the head block
        // attemptMine();

        // // attempt to introduce a new transaction
        attemptTxn();
    }

    void BitcoinPeer::endOfRound(std::vector<Peer*>& peers) {
        std::vector<BitcoinPeer*> bpeers = reinterpret_cast<std::vector<BitcoinPeer*>&>(peers);
        QUANTAS_LOG_INFO("eor") << "End of Round " + std::to_string(RoundManager::currentRound() - 1) + ".";
        int txsMade = 0;
        int msgsSent = 0;
        for (BitcoinPeer* bp : bpeers) {
            txsMade += bp->txnsMadeThisRound_;
            msgsSent += bp->msgsSentThisRound_;
            bp->txnsMadeThisRound_ = 0;
            bp->msgsSentThisRound_ = 0;
        }
        QUANTAS_LOG_DEBUG("eor") << "Transactions made: " + std::to_string(txsMade);
        QUANTAS_LOG_DEBUG("eor") << "Messages sent: " + std::to_string(msgsSent) << std::endl;
        // count blocks mined later
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
                ++txnsMade_; ++txnsMadeThisRound_;
            }
            else {
                newTx = createNewTransaction(publicId(), -1);
                ++txnsMade_; ++txnsMadeThisRound_;
            }
            json newTxMsg = buildTxnMsg(newTx);
            std::string debugOutput = "Transaction Made, Source: " + std::to_string(newTx.source) + ", ID: " + std::to_string(newTx.id) + ", Recipient: " + std::to_string(newTx.receiver);
            QUANTAS_LOG_DEBUG("attemptTxn") << debugOutput;
            broadcast(newTxMsg);
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
        auto it = mempool_.find(t);
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
        auto it = mempool_.find(tempTxn);
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
        auto it = mempool_.find(tempTxn);
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
            // deal with specifics based on the message type
            if (msg["type"] == MessageType::HEADER) {
                int blockId = msg["content"]["block_id"];
                interfaceId minerId = msg["content"]["miner_id"];
                if (!hasBlock(blockId, minerId)) {
                    Block tempBlock;
                    tempBlock.id = blockId;
                    tempBlock.miner = minerId;
                    unicastTo(buildGetDataMsg(tempBlock), p.sourceId());
                    ++msgsSentThisRound_;
                    isWaiting_ = true;
                }
                // else just ignore the header
            }
            else if (msg["type"] == MessageType::GET_DATA) {
                int blockId = msg["content"]["block_id_requested"];
                interfaceId minerId = msg["content"]["miner_id"];
                if (hasBlock(blockId, minerId)) {
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
                if (it != hbnNeighbors_.end()) {
                    // need to build block off of it
                    newBlock = buildBlockFromMsg(msg);
                }
                // if the CMP_BLOCK was sent in response to a GET_DATA
                else if (it == hbnNeighbors_.end() && isWaiting_) {
                    isWaiting_ = false;
                    newBlock = buildBlockFromMsg(msg);
                }
                // else, drop it. (unwanted unsolicited CMP_BLOCK)
                else {
                    continue;
                }

                // if the node does not have all of the transactions listed in the block
                if (!hasAllCmpBlockTxns(msg)) {
                    // insert the current block
                    knownBlocks_.insert(newBlock);
                    // need to send GET_BLOCK_TXN request
                    unicastTo(buildGetBlockTxnMsg(newBlock), p.sourceId());
                    ++msgsSentThisRound_;
                    isWaiting_ = true;
                }
                else {
                    for (const auto& t : msg["content"]["tx_ids"]) {
                        Transaction tempTxn = getStoredTxn(t[1], t[0]);
                        if (!newBlock.isFull()) {
                            newBlock.txns.insert(tempTxn);
                        }
                    }
                    // insert new block, set as tip, and create new candidate
                    knownBlocks_.insert(newBlock);
                    tip_ = newBlock;
                    candidate_ = createNewBlock();
                }

            }
            else if (msg["type"] == MessageType::GET_BLOCK_TXN) {
                int blockId = msg["content"]["block_txn_requested"];
                interfaceId minerId = msg["content"]["miner_id"];
                if (hasBlock(blockId, minerId)) {
                    Block tempBlock = getStoredBlock(blockId, minerId);
                    unicastTo(buildBlockTxnMsg(tempBlock), p.sourceId());
                    ++msgsSentThisRound_;
                }
            }
            else if (msg["type"] == MessageType::BLOCK_TXN) {
                if (isWaiting_) {
                    // pull block from known blocks
                    int blockId = msg["content"]["block_id"];
                    interfaceId minerId = msg["content"]["miner_id"];
                    if (hasBlock(blockId, minerId)) {
                        Block newBlock = getStoredBlock(blockId, minerId);
                        for (const auto& t : getBlockTxnsFromMessage(msg)) {
                            if (!newBlock.isFull()) {
                                newBlock.txns.insert(t);
                            }
                        }
                        // replace old block with updated transactions
                        knownBlocks_.erase(newBlock);
                        knownBlocks_.insert(newBlock);
                        tip_ = newBlock;
                        candidate_ = createNewBlock();
                        isWaiting_ = false;
                    }
                }
                // else do nothing!! weird propagation
            }
            else if (msg["type"] == MessageType::TXN) {
                Transaction incomingTxn = buildTxnFromMsg(msg);
                mempool_.insert(incomingTxn);
                std::string debugOutput = 
                    "Node " + std::to_string(publicId()) + 
                    " recieved TXN ID: " + std::to_string(incomingTxn.id) + 
                    " from " + std::to_string(p.sourceId()) + 
                    ". (delay: " + std::to_string(p.getDelay()) + ")";
                QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
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
    json BitcoinPeer::buildGetDataMsg(const Block& b) const {
        json msg;
        msg["type"] = MessageType::GET_DATA;
        msg["content"]["block_id_requested"] = b.id;
        msg["content"]["miner_id"] = b.miner;
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
            json combinedId = {
                {t.source, t.id}
            };
            msg["content"]["tx_ids"] += combinedId;
        }
        return msg;
    }

    // requires: block_tx_requested
    json BitcoinPeer::buildGetBlockTxnMsg(const Block& b) const {
        json msg;
        msg["type"] = MessageType::GET_BLOCK_TXN;
        msg["content"]["block_txn_requested"] = b.id;
        msg["content"]["miner_id"] = b.miner;
        return msg;
    }

    // requires: txs (bunch of TXN messages)
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
        for (const auto& t : msg["content"]["tx_ids"]) {
            if (!hasTxn(t[1], t[0])) {
                return false;
            }
        }
        return true;
    }

}