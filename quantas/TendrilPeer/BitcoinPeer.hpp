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

        int id = -1;
        int roundCreated = -1;
        interfaceId source;
        interfaceId receiver;
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

        int id = -1;
        int prevId = -1;
        int roundMined = -1;
        interfaceId miner;

        std::set<Transaction> txs;
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
        int tipId_;
        Block candidate_;

        // block/tx creation variables
        double mineProbability_ = 0.01;
        double txProbability_ = 0.2;
        int txsMade_ = 0;
        int blocksMined_ = 0;

        // block/tx creation functions
        Block createNewBlock();
        Transaction createNewTransaction(const interfaceId& sourceId, const interfaceId& receiverId);
        void attemptMine();             // attempt to mine current candidate
        void attemptTx();               // attempt to create new transaction

        // block/tx helper functions
        Block hasBlock(const Block&) const;
        Transaction hasTx(const Transaction&) const;

        // msg sending/receiving
        void checkInStream();
        json buildHeaderMsg(const Block& b) const;
        json buildGetDataMsg(const int& id) const;
        json buildCmpBlockMsg(const Block& b) const;
        json buildGetBlockTxnMsg(const int& id) const;
        json buildBlockTxnMsg(const Block& b) const;
        json buildTxnMsg(const Transaction& t) const;
        Block buildBlockFromMsg(const json& msg) const;
        Block buildTxnFromMsg(const json& msg) const;
        int msgsSentThisRound_ = 0;

        // network variables
        std::vector<interfaceId> hbnNeighbors_;     // tendrilStaller specific: HBN neighbors
    };
}

#endif // TENDRIL_BITCOIN_HPP