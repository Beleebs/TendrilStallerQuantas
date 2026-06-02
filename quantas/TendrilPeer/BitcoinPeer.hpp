// Ben Leber
// 5/20/2026
// Interface based off of the v0.10.0 version of Bitcoin 

#ifndef TENDRIL_BITCOIN_HPP
#define TENDRIL_BITCOIN_HPP

#include <vector>
#include <string>
#include <set>
#include <iostream>

#include "../Common/PowPeer.hpp"
#include "../Common/Pow.hpp"

namespace quantas {
    class BitcoinPeer : public PoWPeer {
    public:
        // constructors/destructor
        BitcoinPeer(NetworkInterface* interfacePtr);
        BitcoinPeer(const BitcoinPeer& rhs);
        ~BitcoinPeer() override = default;

        // quantas specific peer functions
        // PoWPeer adds the runProtocolStep function
        void initParameters(const std::vector<Peer*>& peers, json parameters) override;
        void performComputation() override;
        void runProtocolStep(const std::vector<std::string>& overrideParents) override;
        void endOfRound(std::vector<Peer*>& peers) override;

    private:
        // checks the input stream for broadcasted messages/blocks/transactions/anything i guess
        void checkInStream();

        // could include something that logs the blocks into the output json file
        void logBlock();
        
        // needs:
        // HBN List
        std::vector<Peer*> hbnList = {};
        // UTXO set
        std::set<std::string> utxoSet = {};

        // Reference/pointer to blockchain?
        // Mining Target Value
        std::string target = "000111";
        // thats all i can think of for now
        
    };
}

#endif // TENDRIL_BITCOIN_HPP