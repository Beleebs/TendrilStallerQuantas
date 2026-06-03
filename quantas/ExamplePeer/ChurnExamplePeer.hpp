/*
Copyright 2024

This file is part of QUANTAS.
QUANTAS is free software: you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation, either
version 3 of the License, or (at your option) any later version.
QUANTAS is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with QUANTAS.
If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef CHURNEXAMPLEPEER_HPP
#define CHURNEXAMPLEPEER_HPP

#include <set>
#include <string>
#include <vector>

#include "../Common/Peer.hpp"

namespace quantas {

class Packet;

class ChurnExamplePeer : public Peer {
public:
    explicit ChurnExamplePeer(NetworkInterface* networkInterface);
    ChurnExamplePeer(const ChurnExamplePeer& rhs);
    ~ChurnExamplePeer() override;

    void initParameters(const std::vector<Peer*>& peers, json parameters) override;
    void performComputation() override;
    void endOfRound(std::vector<Peer*>& peers) override;
    void endOfExperiment(std::vector<Peer*>& peers) override;

    int msgsSent = 0;

private:
    struct ChurnEvent {
        std::string action;
        interfaceId peerId = NO_PEER_ID;
        size_t step = 0;
        bool applied = false;
    };

    void checkInStrm();
    void logInboundMessage(const Packet& packet) const;
    json buildGreetingPayload() const;
    void applyInitialMembership(std::vector<Peer*>& peers);
    void applyChurnEvents(std::vector<Peer*>& peers);
    Peer* findLocalPeerById(std::vector<Peer*>& peers, interfaceId peerId) const;
    void disconnectPeerId(std::vector<Peer*>& peers, interfaceId peerId) const;
    void reconnectPeerId(std::vector<Peer*>& peers, interfaceId peerId) const;
    void logMembershipEvent(const std::string& event, interfaceId peerId, const Peer* localPeer) const;
    void logSentMessages(const std::vector<Peer*>& peers) const;

    std::set<interfaceId> _initiallyPresentIds;
    std::set<interfaceId> _initiallyAbsentIds;
    std::vector<ChurnEvent> _events;
    size_t _step = 0;
    bool _initialMembershipApplied = false;
};

} // namespace quantas

#endif // CHURNEXAMPLEPEER_HPP
