/*
Copyright 2024

This file is part of QUANTAS.
QUANTAS is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
QUANTAS is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with QUANTAS. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef CHORDPEER_HPP
#define CHORDPEER_HPP

#include <set>
#include <vector>

#include "../Common/Peer.hpp"

namespace quantas {

class ChordPeer : public Peer {
public:
    ChordPeer(NetworkInterface* interfacePtr);
    ChordPeer(const ChordPeer& rhs);
    ~ChordPeer() override = default;

    void initParameters(const std::vector<Peer*>& peers, json parameters) override;
    void performComputation() override;
    void endOfRound(std::vector<Peer*>& peers) override;
    void endOfExperiment(std::vector<Peer*>& peers) override;

private:
    struct FingerEntry {
        interfaceId nodeId = NO_PEER_ID;
    };

    void checkInStrm();
    void handleLookup(json msg);
    void submitLookup(int transactionId);
    json makeLookupTemplate(interfaceId target, int transactionId) const;
    interfaceId pickRandomTarget() const;
    interfaceId selectFinger(interfaceId target, const std::set<interfaceId>& neighborSet) const;
    interfaceId chooseClockwiseNeighbor(interfaceId target, const std::set<interfaceId>& neighborSet) const;
    void dispatchLookup(json msg, interfaceId nextHop, const std::set<interfaceId>& neighborSet);
    void buildFingerTable();

    std::vector<FingerEntry> _fingers;
    int _networkSize = 1;
    bool _initialized = false;

    int _requestsSatisfied = 0;
    int _totalHops = 0;
    int _totalLatency = 0;
    int _maxHops = 0;
    int _stopAfterSatisfiedRequests = -1;
    bool _stopRequested = false;

    static int s_nextTransactionId;
};

}

#endif // CHORDPEER_HPP
