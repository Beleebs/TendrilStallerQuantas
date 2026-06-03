// Ben Leber
// 5/20/2026
// Interface based off of the v0.10.0 version of Bitcoin 

#ifndef TENDRIL_BITCOIN_HPP
#define TENDRIL_BITCOIN_HPP

#include <vector>
#include <string>
#include <set>
#include <unordered_set>
#include <iostream>

#include "../Common/Peer.hpp"
#include "../Common/PowPeer.hpp"
#include "../Common/Pow.hpp"

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
        // transaction interface
        struct Tx {
            int id = -1;
            int roundSent = -1;
            interfaceId sender = NO_PEER_ID;
            interfaceId receiver = NO_PEER_ID;
        };

        // block structure
        struct Bk {
            // header information
            int id = -1;
            Bk* prev;
            int roundMined = -1;

            // tx information
            int numTx = 0;
            std::set<Tx> txs;
        };

        // checks the input stream for broadcasted messages/blocks/transactions/anything i guess
        void checkInStream();

        // could include something that logs the blocks into the output json file
        void logMinedBlock(const Bk&);
        // logs a transaction into a block
        void logTxIntoBlock(const Tx&, const Bk&);
        
        // builds json message for logMinedBlock
        json buildBlockMessage(const Bk&);
        // builds json message for logTxIntoBlock
        json buildTxMessage(const Tx&);

        // Known Blocks
        std::unordered_set<Bk> knownBlocks_;
        // Known Transactions
        std::unordered_set<Tx> knownTxs_;

        // we want some sort of way to mine a block
        // i think the input should have some sort of modifyable probability value in the input
        double mineProbability_ = 0.05;
        // number of blocks mined by this node
        int blocksMined_ = 0;

        // For this implementation, BIP 152 (or Compact Block Relay) uses a list of neighbors that can
        // send over compact blocks before fully confirming the block's validity. [TODO]

        // This has a fixed size of 3 peers, as the attackers in TendrilStaller require monopoly of a HBN list.
        // std::vector<Peer*> highBandwidthNeighbors_;
        
        // determines whether or not a node transmits at a high bandwidth or low bandwidth
        bool isHighBandwidth_ = false;
    };
}

#endif // TENDRIL_BITCOIN_HPP