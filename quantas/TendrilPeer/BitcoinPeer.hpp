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
    
    private:
        // Transaction interface
        struct Tx {
            // comparison overloading
            // less than
            bool operator<(const Tx& rhs) {
                return roundSent < rhs.roundSent;
            }
            // equals
            bool operator==(const Tx& rhs) {
                return id == rhs.id && roundSent == rhs.roundSent;
            }

            // Transaction ID
            int id = -1;
            // Round it was sent on
            int roundSent = -1;

            // Currently, this is not representative of ACTUAL bitcoin.
            // Simply because Tx's have input/output fields, not necessarily sender/receiver.
            // for simulation sake, just deal w/ it for now
            interfaceId sender = NO_PEER_ID;
            interfaceId receiver = NO_PEER_ID;
        };

        // block structure
        struct Bk {
            // based on time/round mined
            bool operator<(const Bk& rhs) {
                return roundMined < rhs.roundMined;
            }

            bool operator==(const Bk& rhs) {
                return id == rhs.id && txs == rhs.txs;
            }

            // header information
            int id = -1; 
            // used for determining fork resolution
            int height = -1; 
            // previous id
            int prevID;
            // round block was mined
            int roundMined = -1;

            // tx information
            // number of transactions (basically txs.size)
            int numTx = 0;
            // transactions
            std::set<Tx> txs;
            // coinbase transaction (the first tx in the block)
            // not sure if this is specifically the best way to do this? ¯\_(ツ)_/¯
            Tx cbTx;
        };

        // INPUT CHECK FUNCTIONS
        // checks the input stream for broadcasted messages/blocks/transactions/anything i guess
        void checkInStream();

        // MESSAGE LOGGING FUNCTIONS
        // could include something that logs the blocks into the output json file
        void logMinedBlock(const Bk&);
        // builds json message for blocks
        json buildBlockMessage(const Bk&);
        // builds json message for transactions
        json buildTxMessage(const Tx&);

        // BLOCKCHAIN SPECIFIC FUNCTIONS
        // Known Blocks
        std::unordered_map<int, Bk> knownBlocks_;
        // Known Transactions
        std::unordered_map<int, Tx> knownTxs_;
        // Mempool for unconfirmed transactions
        std::set<Tx> mempool_;
        // contains the id of the current block being mined
        int topBlockID_ = -1;

        // probability for a block to be mined by the node
        double mineProbability_ = 0.05;
        // number of blocks mined by this node
        int blocksMined_ = 0;

        // probability for transaction to be made
        double txProbability_ = 0.2;

        // TODO: implement compact block relay
        // For this implementation, BIP 152 (or Compact Block Relay) uses a list of neighbors that can
        // send over compact blocks before fully confirming the block's validity.
        // This has a fixed size of 3 peers, as the attackers in TendrilStaller require monopoly of a HBN list.
        // std::vector<Peer*> highBandwidthNeighbors_;
        // Determines whether or not a node transmits at a high bandwidth or low bandwidth
        //bool isHighBandwidth_ = false;
    };
}

#endif // TENDRIL_BITCOIN_HPP