// Ben Leber
// 5/20/2026
// Interface based off of the v0.10.0 version of Bitcoin 

#ifndef TENDRIL_BITCOIN_HPP
#define TENDRIL_BITCOIN_HPP

#include <vector>
#include <string>
#include <set>
#include <unordered_set>
#include <deque>
#include <iostream>

#include "../Common/Peer.hpp"
#include "../Common/Packet.hpp"

namespace quantas {

    enum class MessageType {
        HEADER,
        GET_DATA,
        CMP_BLOCK,
        GET_BLOCK_TXN,
        BLOCK_TXN,
        TXN
    };

    struct Transaction {
        bool operator<(const Transaction& rhs) const {
            if (source == rhs.source) {
                return id < rhs.id;
            }
            return source < rhs.source;
        }

        bool operator==(const Transaction& rhs) const {
            return id == rhs.id && source == rhs.source;
        }

        int id = -2;
        int roundCreated = -2;
        interfaceId source = -2L;
        interfaceId receiver = -2L;
    };

    struct Block {
        bool operator<(const Block& rhs) const {
            if (miner == rhs.miner) {
                return id < rhs.id;
            }
            return miner < rhs.miner;
        }

        bool operator==(const Block& rhs) const {
            return id == rhs.id && miner == rhs.miner;
        }

        // limit of 10 txns per block
        bool isFull() const {
            return txns.size() > 10;
        }

        int id = -2;
        int prevId = -2;
        int roundMined = -2;
        int height = -2;
        interfaceId miner = -2L;

        std::set<Transaction> txns;
    };

    class BitcoinPeer : public Peer {
    public:
        // constructors/destructor
        BitcoinPeer(NetworkInterface* interfacePtr);
        BitcoinPeer(const BitcoinPeer& rhs);
        ~BitcoinPeer() override = default;

        // quantas specific peer functions
        void initParameters(const std::vector<Peer*>& peers, json parameters) override;
        void performComputation() override;
        void endOfRound(std::vector<Peer*>& peers) override;
        void endOfExperiment(std::vector<Peer*>& peers) override;
    
    private:
        // miner blockchain state variables
        std::set<Block> knownBlocks_;
        std::set<Transaction> mempool_;
        Block tip_;
        Block candidate_;
        bool isWaiting_ = false;        // locks instream if waiting for CMP_BLOCK or BLOCK_TXN

        // block/txn creation variables
        double mineProbability_ = 0.01;
        double txnProbability_ = 0.2;
        int txnsMade_ = 0;
        int blocksMined_ = 0;

        // block chain helper functions
        // something later

        // block/txn creation functions
        Block createNewBlock();
        Transaction createNewTransaction(const interfaceId& sourceId, const interfaceId& receiverId);
        void attemptMine();             // attempt to mine current candidate
        void attemptTxn();              // attempt to create new transaction
        int txnsMadeThisRound_ = 0;

        // block/txn helper functions
        bool hasBlock(const Block&) const;
        bool hasBlock(const int& id, const interfaceId& minerId) const;
        bool hasTxn(const Transaction&) const;
        bool hasTxn(const int& id, const interfaceId& sourceId) const;
        Block getStoredBlock(const int& id, const interfaceId& minerId) const;
        Transaction getStoredTxn(const int& id, const interfaceId& sourceId) const;

        // msg sending/receiving
        void checkInStream();
        json buildHeaderMsg(const Block& b) const;
        json buildGetDataMsg(const Block& b) const;
        json buildCmpBlockMsg(const Block& b) const;
        json buildGetBlockTxnMsg(const Block& b) const;
        json buildBlockTxnMsg(const Block& b) const;
        json buildTxnMsg(const Transaction& t) const;
        Block buildBlockFromMsg(const json& msg) const;
        Transaction buildTxnFromMsg(const json& msg) const;
        std::set<Transaction> getBlockTxnsFromMessage(const json& msg) const;
        bool hasAllCmpBlockTxns(const json& msg) const;
        int msgsSentThisRound_ = 0;

        // network variables
        std::set<interfaceId> hbnNeighbors_;     // tendrilStaller specific: HBN neighbors
    };
}

#endif // TENDRIL_BITCOIN_HPP