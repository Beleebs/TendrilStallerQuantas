// Ben Leber
// 6/23/2026
// Bitcoin Peer Implementation: COMPACT BLOCK DELAY

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
            // QUANTAS_LOG_INFO("ip") << s;
        }
        std::string finalMsg = "All (" + std::to_string(bpeers.size()) + ") nodes ready to go.\n";
        // QUANTAS_LOG_INFO("ip") << finalMsg;
        json msg = {
            {"max_msg_delay", parameters.value("maxDelay", 1)},
            {"mine_probability", parameters.value("mineProbability", 0.003)},
            {"txn_probability", parameters.value("txnProbability", 0.05)},
            {"packet_loss_percentage", parameters.value("packetLossPercentage", 0.0)},
            {"number_peers", parameters.value("peers", 10)}
        };
        OutputWriter::pushValue("parameters", msg);
    }

    void BitcoinPeer::performComputation() {
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
        int blocksMined = 0;
        for (BitcoinPeer* bp : bpeers) {
            txsMade += bp->txnsMadeThisRound_;
            msgsSent += bp->msgsSentThisRound_;
            blocksMined += bp->blocksMinedThisRound_;
            bp->txnsMadeThisRound_ = 0;
            bp->msgsSentThisRound_ = 0;
            bp->blocksMinedThisRound_ = 0;

            // check for newly verified blocks based on previous round height
            if (bp->prevRoundTipHeight_ < bp->tip_.height) {
                // check if any blocks in the current chain are confirmed
                Block current = bp->tip_;
                while (current.prevId != -2 && current.prevMiner != -2 && current.height > bp->prevRoundTipHeight_ - 6) {
                    if (bp->isConfirmedBlock(current)) {
                        // log block
                        bp->logConfirmedBlock(current);
                        // can also remove all of the transactions found in those confirmed blocks to make the process faster? (clear up mempool space)
                        bp->removeConfirmedTransactions(current);
                    }
                    current = bp->getStoredBlock(current.prevId, current.prevMiner);
                }
            }
            // update prevRoundTipHeight afterwards
            bp->prevRoundTipHeight_ = bp->tip_.height;
        }

        // QUANTAS_LOG_INFO("eor") << "End of Round " + std::to_string(RoundManager::currentRound() - 1) + ". Transactions made: " + std::to_string(txsMade) + ", Messages sent: " + std::to_string(msgsSent) + ", Blocks mined: " + std::to_string(blocksMined) << std::endl;
        OutputWriter::pushValue("txs_sent_per_round", txsMade);
        OutputWriter::pushValue("msgs_sent_per_round", msgsSent);
        OutputWriter::pushValue("blocks_mined_per_round", blocksMined);
    }

    void BitcoinPeer::endOfExperiment(std::vector<Peer*>& peers) {
        std::vector<BitcoinPeer*> bpeers = reinterpret_cast<std::vector<BitcoinPeer*>&>(peers);
        int txsMade = 0;
        int blocksMined = 0;
        std::set<Block> allLocalLedgerBlocks;

        for (BitcoinPeer* bp : bpeers) {
            txsMade += bp->txnsMade_;
            blocksMined += bp->blocksMined_;
            bp->logLedger();
            // bp->logTxnConfirmationDelays();
            Block current = bp->tip_;
            while (current.prevId != -2 && current.prevMiner != -2 && !bp->isConfirmedBlock(current)) {
                bp->logUnconfirmedBlock(current);
                current = bp->getStoredBlock(current.prevId, current.prevMiner);
            }
        }
        
        OutputWriter::pushValue("total_txs_made", txsMade);
        OutputWriter::pushValue("total_blocks_mined", blocksMined);
    }

    bool BitcoinPeer::isChainComplete(const Block& b) const {
        // go backwards until the genesis (where prevId is -2, prevMiner is -2)
        Block current = b;
        while (current.prevId != -2 || current.prevMiner != -2) {
            if (!hasBlock(current.prevId, current.prevMiner)) {
                return false;
            }
            current = getStoredBlock(current.prevId, current.prevMiner);
        }
        return true;
    }

    Block BitcoinPeer::findBestChainTip() const {
        Block bestTip = tip_;
        int bestHeight = tip_.height;
        // goes through each known block, finding a new potential best chain
        for (const auto& block : knownBlocks_) {
            if (!isChainComplete(block)) continue;
            int height = block.height;
            // compares on height and chronological occurrence (partial ordering to a node's blocks mined (very confusing to say, yeah...))
            if (height > bestHeight || (height == bestHeight && std::tie(block.miner, block.id) < std::tie(bestTip.miner, bestTip.id))) {
                bestTip = block;
                bestHeight = height;
            }
        }

        return bestTip;
    }

    void BitcoinPeer::adoptChain(const Block& newTip) {
        if (newTip == tip_) {
            return;
        }
        tip_ = newTip;
        candidate_ = createNewBlock();
    }

    // outdated, incorrectly identifies transaction confirmation delays
    void BitcoinPeer::logTxnConfirmationDelays() const {
        json normalTxns;
        json coinbaseTxns;
        json unconfirmedBlockTxns;
        json unconfirmedTxns;

        int normalCntr = 1, normalAvg = 0;
        int coinbaseCntr = 1, coinbaseAvg = 0;
        int unconfBlockTxnsCntr = 1, unconfBlockTxnsAvg = 0;
        int unconfTxnsCntr = 1, unconfTxnsAvg = 0;

        std::set<Transaction> inBlockTransactions;

        for (const auto& b : knownBlocks_) {
            if (isConfirmedBlock(b)) {
                for (const auto& t : b.txns) {
                    if (t.source != -1) {
                        ++normalCntr;
                        normalAvg += (b.roundMined - t.roundCreated);
                    }
                    else {
                        ++coinbaseCntr;
                        coinbaseAvg += (b.roundMined - t.roundCreated);
                    }
                    inBlockTransactions.insert(t);
                }
            }
            else {
                for (const auto& t : b.txns) {
                    ++unconfBlockTxnsCntr;
                    unconfBlockTxnsAvg += (b.roundMined - t.roundCreated);
                    inBlockTransactions.insert(t);
                }
            }
        }

        normalTxns["count"] = json::number_integer_t(normalCntr);
        normalTxns["confirmation_delay_avg"] = json::number_integer_t(normalAvg / normalCntr);
        coinbaseTxns["count"] = json::number_integer_t(coinbaseCntr);
        coinbaseTxns["confirmation_delay_avg"] = json::number_integer_t(coinbaseAvg / coinbaseCntr);
        unconfirmedBlockTxns["count"] = json::number_integer_t(unconfBlockTxnsCntr);
        unconfirmedBlockTxns["rounds_unconfirmed_avg"] = json::number_integer_t(unconfBlockTxnsAvg / unconfBlockTxnsCntr);

        for (const Transaction& t : mempool_) {
            if (inBlockTransactions.find(t) == inBlockTransactions.end()) {
                ++unconfTxnsCntr;
                unconfTxnsAvg += (RoundManager::currentRound() - t.roundCreated);
            }
        }

        unconfirmedTxns["count"] = json::number_integer_t(unconfTxnsCntr);
        unconfirmedTxns["rounds_unconfirmed_avg"] = json::number_integer_t(unconfTxnsAvg / unconfTxnsCntr);


        OutputWriter::pushValue("txn_confirmation_delay", normalTxns);
        OutputWriter::pushValue("cb_txn_confirmation_delay", coinbaseTxns);
        OutputWriter::pushValue("unconfirmed_block_txns", unconfirmedBlockTxns);
        OutputWriter::pushValue("unconfirmed_txns", unconfirmedTxns);
    }

    // 
    bool BitcoinPeer::isConfirmedBlock(const Block& b) const {
        Block current = tip_;
        int depth = 0;
        // ensures that we go from current tip to end, primarily focusing on longest chain
        while (current.prevId != -2 || current.prevMiner != -2) {
            if (current == b) {
                return depth > 5;
            }

            if (!hasBlock(current.prevId, current.prevMiner)) {
                return false;
            }

            current = getStoredBlock(current.prevId, current.prevMiner);
            ++depth;
        }
        return false;
    }

    void BitcoinPeer::removeConfirmedTransactions(const Block& b) {
        for (const Transaction& t : b.txns) {
            auto it = std::find(mempool_.begin(), mempool_.end(), t);
            if (it != mempool_.end()) {
                mempool_.erase(it);
            }
        }
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
        // ++txnsMadeThisRound_;   // debug info
        return b;
    }

    Transaction BitcoinPeer::createNewTransaction(const interfaceId& sourceId, const interfaceId& receiverId) {
        Transaction t;
        t.id = txnsMade_;
        t.roundCreated = RoundManager::currentRound();
        t.source = sourceId;
        t.receiver = receiverId;
        // ++txnsMade_;
        return t;
    }

    void BitcoinPeer::attemptMine() {
        int random = rand() % 1000 + 1;
        if (random >= 1000 - (1000 * mineProbability_)) {
            // debug
            std::string debugOutput = 
                "\t\tBlock mined, ID: " + std::to_string(candidate_.id) + 
                ", Miner ID: " + std::to_string(candidate_.miner) + 
                ", Height: " + std::to_string(candidate_.height);
            QUANTAS_LOG_DEBUG("attemptMine") << debugOutput;

            // insert correct block transactions
            insertValidTransactions(candidate_);
            // QUANTAS_LOG_TRACE("attemptMine") << "After insertValidTransactions()";

            // block has been mined
            candidate_.roundMined = RoundManager::currentRound();
            knownBlocks_.insert(candidate_);
            tip_ = candidate_;
            ++blocksMined_; ++blocksMinedThisRound_;
            Block newCandidate = createNewBlock();
            // tip_ = candidate_;  // update tip after creating new block?
            // QUANTAS_LOG_TRACE("attemptMine") << "After creating new block";

            // build header message
            broadcast(buildHeaderMsg(candidate_));
            msgsSentThisRound_ += neighbors().size();
            // QUANTAS_LOG_TRACE("attemptMine") << "After header broadcast";

            // finally update state and continue
            candidate_ = newCandidate;
            // QUANTAS_LOG_TRACE("attemptMine") << "After updating new candidate";
        }
    }

    void BitcoinPeer::insertValidTransactions(Block& b) {
        // parse current blockchain
        std::set<Transaction> foundTxns;
        Block current = tip_;
        // QUANTAS_LOG_TRACE("insertValidTransactions") << "Before parsing blockchain";
        while (current.prevId != -2 || current.prevMiner != -2) {
            // QUANTAS_LOG_TRACE("insertValidTransactions") << "current block id: " << current.id << ", miner id: " << current.miner;
            Block prevBlock = getStoredBlock(current.prevId, current.prevMiner);
            for (const auto& t : current.txns) {
                foundTxns.insert(t);
            }
            current = prevBlock;
        }

        // QUANTAS_LOG_TRACE("insertValidTransactions") << "Before inserting txns";
        // until the current block is full, insert transactions that aren't found in foundTxns
        for (const auto& t : mempool_) {
            if (!b.isFull()) {
                auto it = foundTxns.find(t);
                if (it == foundTxns.end()) {
                    b.txns.insert(t);
                    // QUANTAS_LOG_TRACE("insertValidTransactions") << "inserting txn id: " << t.id << " from source id: " << t.source << ". New block size: " << b.txns.size();
                }
            }
            else {
                // QUANTAS_LOG_TRACE("insertValidTransactions") << "block full, exiting...";
                break;
            } 
        }

        // QUANTAS_LOG_TRACE("insertValidTransactions") << "end of insertValidTransactions";
    }

    void BitcoinPeer::attemptTxn() {
        int random = rand() % 1000 + 1;
        // transaction probability is hit
        if (random >= 1000 - (1000 * txnProbability_)) {
            Transaction newTx;
            std::set<interfaceId> neighborSet = neighbors();
            if (!neighborSet.empty()) {
                int index = rand() % neighbors().size();
                auto it = neighborSet.begin();
                std::advance(it, index);
                newTx = createNewTransaction(publicId(), *it);
                mempool_.push_back(newTx);
                ++txnsMadeThisRound_; ++txnsMade_;
            }
            else {
                newTx = createNewTransaction(publicId(), -1);
                mempool_.push_back(newTx);
                ++txnsMadeThisRound_; ++txnsMade_;
            }
            json newTxMsg = buildTxnMsg(newTx);
            std::string debugOutput = "Transaction Made, Source: " + std::to_string(newTx.source) + ", ID: " + std::to_string(newTx.id) + ", Recipient: " + std::to_string(newTx.receiver);
            // QUANTAS_LOG_DEBUG("attemptTxn") << debugOutput;
            broadcast(newTxMsg);     // standard output, hard to cause GET_BLOCK_TXNs
            // randomMulticast(newTxMsg);  // used for creating GET_BLOCK_TXN requests
            msgsSentThisRound_ += neighbors().size();
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

    void BitcoinPeer::checkInStream() {
        while (!inStreamEmpty()) {
            Packet p = popInStream();
            OutputWriter::pushValue("msg_delay", p.getDelay());   // logging msg delay values
            json msg = p.getMessage();
            std::string debugOutput;
            // deal with specifics based on the message type
            if (msg["type"] == MessageType::HEADER) {
                // debug
                debugOutput = 
                    "Node " + std::to_string(publicId()) + 
                    " recieved HEADER from " + std::to_string(p.sourceId()) + 
                    ". (delay: " + std::to_string(p.getDelay()) + ")";
                // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                

                int blockId = msg["content"]["block_id"];
                interfaceId minerId = msg["content"]["miner_id"];
                if (!hasBlock(blockId, minerId)) {
                    auto blockKey = std::make_pair(blockId, minerId);
                    if (pendingBlocks_.find(blockKey) == pendingBlocks_.end()) {
                        // debug
                        debugOutput = 
                            "New block, block ID: " + std::to_string(blockId) +
                            ", miner ID: " + std::to_string(minerId);
                        // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                        Block tempBlock;
                        tempBlock.id = blockId;
                        tempBlock.miner = minerId;
                        unicastTo(buildGetDataMsg(tempBlock), p.sourceId());
                        ++msgsSentThisRound_;
                        pendingBlocks_.insert(std::make_pair(blockId, minerId));
                    }
                }
                // else just ignore the header
                else {
                    // debug
                    debugOutput = "Block already exists.";
                    // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                    continue;
                }
            }
            else if (msg["type"] == MessageType::GET_DATA) {
                // debug
                debugOutput = 
                    "Node " + std::to_string(publicId()) + 
                    " recieved GET_DATA from " + std::to_string(p.sourceId()) + 
                    ". (delay: " + std::to_string(p.getDelay()) + ")";
                // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                int blockId = msg["content"]["block_id_requested"];
                interfaceId minerId = msg["content"]["miner_id"];
                if (hasBlock(blockId, minerId)) {
                    // debug
                    debugOutput = 
                        "Block found, sending CMP_BLOCK for Block ID: " + std::to_string(blockId) +
                        " to recipient: " + std::to_string(p.sourceId());
                    // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

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
                Block newBlock;

                // if the CMP_BLOCK was sent in response to a GET_DATA
                auto blockKey = std::make_pair(blockId, minerId);
                if (pendingBlocks_.find(blockKey) != pendingBlocks_.end()) {
                    if (blockId != -2) {
                        // debug
                        debugOutput = 
                            "Node " + std::to_string(publicId()) + 
                            " recieved requested CMP_BLOCK from " + std::to_string(p.sourceId()) + 
                            ". (delay: " + std::to_string(p.getDelay()) + ")";
                        // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                        pendingBlocks_.erase(blockKey);
                        newBlock = buildBlockFromMsg(msg);
                    }
                    else {
                        // debug
                        debugOutput = 
                            "Empty block sent, sender ID " + std::to_string(p.sourceId()) +
                            " does not have requested block.";
                        // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    }
                }
                // else, drop it.
                else {
                    // debug
                    debugOutput = "Received unsolicited and/or stale CMP_BLOCK from: " + std::to_string(p.sourceId()) + "(" + std::to_string(minerId) + ":" + std::to_string(blockId) + ")";
                    // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
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
                    // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    // insert the current block
                    knownBlocks_.insert(newBlock);
                    // need to send GET_BLOCK_TXN request
                    unicastTo(buildGetBlockTxnMsg(newBlock), p.sourceId());
                    ++msgsSentThisRound_;
                    isWaiting_ = true;      // waiting for BlockTxns Message
                    waitingBlockId_ = blockId;
                    waitingMinerId_ = minerId;
                }
                else {
                    // debug
                    debugOutput = 
                        "Node " + std::to_string(publicId()) + 
                        " has all txns found in block id: " + std::to_string(blockId) + 
                        ", from miner id: " + std::to_string(minerId);
                    // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    for (const auto& t : msg["content"]["tx_ids"]) {
                        Transaction tempTxn = getStoredTxn(t["tx_id"], t["source_id"]);
                        if (!newBlock.isFull()) {
                            newBlock.txns.insert(tempTxn);
                        }
                    }
                    // insert new block, set as tip, and create new candidate
                    knownBlocks_.insert(newBlock);
                    OutputWriter::pushValue("block_received_delay", RoundManager::currentRound() - newBlock.roundMined);

                    // new fork logic: find the best tip in knownblocks, then set THAT as the tip
                    // better updating system rather than basing fork resolution on height
                    Block bestTip = findBestChainTip();
                    if (bestTip != tip_) {
                        // debug
                        debugOutput = 
                            "New best tip: block id: " + std::to_string(newBlock.id) + 
                            ", miner id: " + std::to_string(newBlock.miner) +
                            ". Setting as new tip and creating new candidate...";
                        // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                        adoptChain(bestTip);

                        // debug
                        debugOutput =
                            "Tip updated! New candidate id: " + std::to_string(candidate_.id) + 
                            ", miner id: " + std::to_string(candidate_.miner) +
                            ". New height: " + std::to_string(candidate_.height);
                        // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                    }
                    else {
                        if (!hasBlock(newBlock.prevId, newBlock.prevMiner)) {
                            // QUANTAS_LOG_DEBUG("checkInStream") << "newBlock's parent missing, sending GET_DATA...'";
                            unicastTo(buildGetDataMsg(newBlock), p.sourceId());
                            ++msgsSentThisRound_;

                            pendingBlocks_.insert(std::make_pair(newBlock.prevId, newBlock.prevMiner));
                            // isWaiting_ = true;
                            // waitingBlockId_ = newBlock.prevId;
                            // waitingMinerId_ = newBlock.prevMiner;
                        }
                        else {
                            // QUANTAS_LOG_DEBUG("checkInStream") << "newBlock not best tip, keeping current tip...";
                        }
                    }
                }
            }
            else if (msg["type"] == MessageType::GET_BLOCK_TXN) {
                // debug
                debugOutput = 
                    "Node " + std::to_string(publicId()) + 
                    " recieved GET_BLOCK_TXN from " + std::to_string(p.sourceId()) + 
                    ". (delay: " + std::to_string(p.getDelay()) + ")";
                // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                int blockId = msg["content"]["block_txn_requested"];
                interfaceId minerId = msg["content"]["miner_id"];
                if (hasBlock(blockId, minerId)) {
                    // debug
                    debugOutput = 
                        "Sending txns from block id: " + std::to_string(blockId) + 
                        ", from miner id: " + std::to_string(minerId);
                    // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    Block tempBlock = getStoredBlock(blockId, minerId);
                    unicastTo(buildBlockTxnMsg(tempBlock), p.sourceId());
                    ++msgsSentThisRound_;
                }
            }
            else if (msg["type"] == MessageType::BLOCK_TXN) {
                // pull block from known blocks
                int blockId = msg["content"]["block_id"];
                interfaceId minerId = msg["content"]["miner_id"];
                if (isWaiting_) {
                // if (isWaiting_ && blockId == waitingBlockId_ && minerId == waitingMinerId_) {
                    // debug
                    debugOutput = 
                        "Node " + std::to_string(publicId()) + 
                        " recieved BLOCK_TXN from " + std::to_string(p.sourceId()) + 
                        ". (delay: " + std::to_string(p.getDelay()) + ")";
                    // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                    if (hasBlock(blockId, minerId)) {
                        // debug
                        debugOutput = "Block found, inserting new transactions...";
                        // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                        Block newBlock = getStoredBlock(blockId, minerId);
                        for (const auto& t : getBlockTxnsFromMessage(msg)) {
                            if (!newBlock.isFull()) {
                                newBlock.txns.insert(t);
                                if (t.source != -1) {
                                    mempool_.push_back(t);      // also insert into the mempool
                                }
                            }
                        }
                        // replace old block with updated transactions
                        knownBlocks_.erase(newBlock);
                        knownBlocks_.insert(newBlock);
                        OutputWriter::pushValue("block_received_delay", RoundManager::currentRound() - newBlock.roundMined);

                        // Same fork logic
                        Block bestTip = findBestChainTip();
                        if (bestTip != tip_) {
                            // debug
                            debugOutput = 
                                "New best tip: block id: " + std::to_string(newBlock.id) + 
                                ", miner id: " + std::to_string(newBlock.miner) +
                                ". Setting as new tip and creating new candidate...";
                            // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;

                            adoptChain(bestTip);

                            // debug
                            debugOutput =
                                "Tip updated! New tip id: " + std::to_string(tip_.id) + 
                                ", miner id: " + std::to_string(tip_.miner) +
                                ". New height: " + std::to_string(tip_.height);
                            // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
                        }
                        else {
                            // QUANTAS_LOG_DEBUG("checkInStream") << "newBlock not best tip, keeping current tip...";
                        }
                        isWaiting_ = false;
                        waitingBlockId_ = -2;
                        waitingMinerId_ = -2;   
                    }
                }
                // else do nothing!! weird propagation
                else {
                    // debug
                    debugOutput = 
                        "Node " + std::to_string(publicId()) + 
                        " recieved random BLOCK_TXN from " + std::to_string(p.sourceId()) + ".";
                    // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
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
                // QUANTAS_LOG_DEBUG("checkInStream") << debugOutput;
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
        msg["content"]["prev_miner_id"] = b.prevMiner;
        msg["content"]["height"] = b.height;
        msg["content"]["tx_ids"] = json::array();
        
        for (const auto& t : b.txns) {
            if (t.source != -1) {   // ignore coinbase
                json combinedId = {
                    {"source_id", t.source},
                    {"tx_id", t.id}
                };
                msg["content"]["tx_ids"] += combinedId;
            }
            else {
                msg["content"]["coinbase"] = buildTxnMsg(t);
            }
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
            result.prevMiner = msg["content"]["prev_miner_id"];
            result.height = msg["content"]["height"];
            result.txns.insert(buildTxnFromMsg(msg["content"]["coinbase"]));   // insert coinbase transaction
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
        // QUANTAS_LOG_TRACE("hasAllCmpBlockTxns") << "In hasAllCmpBlockTxns for Node: " << publicId();

        if (msg["content"]["tx_ids"].empty()) {
            // QUANTAS_LOG_DEBUG("hasAllCmpBlockTxns") << "Returned True! (empty block)";
            return true;
        }

        for (const auto& t : msg["content"]["tx_ids"]) {
            // QUANTAS_LOG_DEBUG("hasAllCmpBlockTxns") << "Source ID: " << t["source_id"] << ", Txn ID: " << t["tx_id"];

            // coinbase TXN, ignore
            if (t["source_id"] == -1) {
                // QUANTAS_LOG_DEBUG("hasAllCmpBlockTxns") << "\t\tFound CoinBase Txn. Continuing...";
                continue;
            }

            if (!hasTxn(t["tx_id"], t["source_id"])) {
                // QUANTAS_LOG_DEBUG("hasAllCmpBlockTxns") << "Returned False, missing transaction.";
                return false;
            }
        }
        // QUANTAS_LOG_DEBUG("hasAllCmpBlockTxns") << "Returned True!";
        return true;
    }

    void BitcoinPeer::logLedger() const {
        json ledger;

        // // Accurate ledger from tip_
        // Block current = tip_;
        // while (current.prevId != -2 || current.prevMiner != -2) {
        //     Block prevBlock = getStoredBlock(current.prevId, current.prevMiner);
        //     ledger += buildBlockLog(current);
        //     current = prevBlock;
        // }

        // All known blocks
        for (const auto& b : knownBlocks_) {
            ledger += buildBlockLog(b);
        }

        OutputWriter::pushValue("ledgers", ledger);
    }
        
    json BitcoinPeer::buildBlockLog(const Block& b) const {
        std::string blockId = std::to_string(b.miner) + ":" + std::to_string(b.id);

        std::string prevId = std::to_string(b.prevMiner) + ":" + std::to_string(b.prevId);
        json prevIdArr = json::array({prevId});
        // json txArray = json::array();
        // for (const auto& t : b.txns) {
        //     txArray += buildTxnLog(t);
        // }

        json result = {
            {"hash", blockId},
            {"height", b.height},
            {"parents", prevIdArr},
            {"parasite", false},
            {"round_mined", b.roundMined},
            {"size", b.txns.size()},
            // {"txns", txArray}
        };
        return result;
    }

    json BitcoinPeer::buildTxnLog(const Transaction& t) const {
        std::string txnId = std::to_string(t.source) + ":" + std::to_string(t.id);
        json result = {
            {"txn_id", txnId},
            {"round_created", t.roundCreated}
        };
        return result;
    }

    // input block IS confirmed already, so we gotta log it
    void BitcoinPeer::logConfirmedBlock(const Block& b) const {
        std::string outputLabel = "confirmed_blocks_" + std::to_string(publicId());
        json msg;

        int normalCntr = 0;
        int avgTxnConfDelay = 0;

        // QUANTAS_LOG_DEBUG("logConfirmedBlock") << "Block id: " << b.miner << ":" << b.id << ", mined round: " << b.roundMined << ", confirmation delay: " << RoundManager::currentRound() - b.roundMined;
        for (const Transaction& t : b.txns) {
            if (t.source != -1) {
                ++normalCntr;
                avgTxnConfDelay += RoundManager::currentRound() - t.roundCreated;
                // QUANTAS_LOG_DEBUG("logConfirmedBlock") << "\t\tTransaction id: " << t.id << ", created round: " << t.roundCreated << ", confirmation delay: " << RoundManager::currentRound() - t.roundCreated;
            }
        }

        msg["block_confirmation_delay"] = RoundManager::currentRound() - b.roundMined;
        msg["avg_txn_confirmation_delay"] = normalCntr > 0 ? avgTxnConfDelay / normalCntr : avgTxnConfDelay / 1;
        OutputWriter::pushValue(outputLabel, msg);
    }

    void BitcoinPeer::logUnconfirmedBlock(const Block& b) const {
        std::string outputLabel = "unconfirmed_blocks_" + std::to_string(publicId());
        json msg;

        int normalCntr = 0;
        int avgDelay = 0;

        // QUANTAS_LOG_DEBUG("logUnconfirmedBlock") << "Block id: " << b.miner << ":" << b.id << ", mined round: " << b.roundMined << ", rounds waiting for confirmation: " << RoundManager::currentRound() - b.roundMined;
        for (const Transaction& t : b.txns) {
            if (t.source != -1) {
                ++normalCntr;
                avgDelay += RoundManager::currentRound() - t.roundCreated;
                // QUANTAS_LOG_DEBUG("logUnconfirmedBlock") << "\t\tTransaction id: " << t.source << ":" << t.id << ", created round: " << t.roundCreated << ", rounds waiting for confirmation: " << RoundManager::currentRound() - t.roundCreated;
            }
        }
        msg["depth_from_tip"] = tip_.height - b.height;
        msg["rounds_unconfirmed"] = RoundManager::currentRound() - b.roundMined;
        msg["avg_txn_rounds_unconfirmed"] = normalCntr > 0 ? avgDelay / normalCntr : avgDelay / 1;
        OutputWriter::pushValue(outputLabel, msg);
    }
}