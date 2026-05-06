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

#include <limits>
#include <string>

#include "ChurnExamplePeer.hpp"

namespace quantas {

static bool registerChurnExamplePeer = []() {
    return PeerRegistry::registerPeerType(
        "ChurnExamplePeer",
        [](interfaceId pubId) { return new ChurnExamplePeer(new NetworkInterfaceAbstract(pubId)); });
}();

static bool registerChurnExamplePeerConcrete = []() {
    return PeerRegistry::registerPeerType(
        "ChurnExamplePeerConcrete",
        [](interfaceId) { return new ChurnExamplePeer(new NetworkInterfaceConcrete()); });
}();

ChurnExamplePeer::ChurnExamplePeer(NetworkInterface* networkInterface)
    : Peer(networkInterface) {}

ChurnExamplePeer::ChurnExamplePeer(const ChurnExamplePeer& rhs)
    : Peer(rhs) {
    msgsSent = rhs.msgsSent;
    _initiallyPresentIds = rhs._initiallyPresentIds;
    _initiallyAbsentIds = rhs._initiallyAbsentIds;
    _events = rhs._events;
    _step = rhs._step;
    _initialMembershipApplied = rhs._initialMembershipApplied;
}

ChurnExamplePeer::~ChurnExamplePeer() = default;

void ChurnExamplePeer::initParameters(const std::vector<Peer*>& peers, json parameters) {
    if (!parameters.is_object()) {
        parameters = json::object();
    }

    std::set<interfaceId> presentIds;
    std::set<interfaceId> absentIds;
    if (parameters.contains("initiallyPresentPeerIds") && parameters["initiallyPresentPeerIds"].is_array()) {
        for (const auto& value : parameters["initiallyPresentPeerIds"]) {
            if (value.is_number_integer()) presentIds.insert(value.get<interfaceId>());
        }
    }
    if (parameters.contains("initiallyAbsentPeerIds") && parameters["initiallyAbsentPeerIds"].is_array()) {
        for (const auto& value : parameters["initiallyAbsentPeerIds"]) {
            if (value.is_number_integer()) absentIds.insert(value.get<interfaceId>());
        }
    }

    std::vector<ChurnEvent> configuredEvents;
    if (parameters.contains("events") && parameters["events"].is_array()) {
        for (const auto& eventJson : parameters["events"]) {
            if (!eventJson.is_object()) continue;
            ChurnEvent event;
            event.action = eventJson.value("action", std::string());
            event.peerId = eventJson.value("peerId", static_cast<interfaceId>(NO_PEER_ID));
            event.step = eventJson.value("step", static_cast<size_t>(0));
            if ((event.action == "leave" || event.action == "join") && event.peerId != NO_PEER_ID) {
                configuredEvents.push_back(event);
            }
        }
    }

    json schedule = {
        {"initiallyPresentPeerIds", presentIds},
        {"initiallyAbsentPeerIds", absentIds},
        {"events", parameters.value("events", json::array())}
    };
    OutputWriter::pushValue("churnSchedule", schedule);

    for (Peer* peerPtr : peers) {
        auto* churnPeer = dynamic_cast<ChurnExamplePeer*>(peerPtr);
        if (!churnPeer) continue;
        churnPeer->_initiallyPresentIds = presentIds;
        churnPeer->_initiallyAbsentIds = absentIds;
        churnPeer->_events = configuredEvents;
        churnPeer->_step = 0;
        churnPeer->_initialMembershipApplied = false;
    }

    auto mutablePeers = peers;
    applyInitialMembership(mutablePeers);
}

void ChurnExamplePeer::performComputation() {
    OutputWriter::pushValue("performs computation", publicId());
    checkInStrm();

    json message = buildGreetingPayload();
    broadcast(message);
    msgsSent += static_cast<int>(neighbors().size());
}

void ChurnExamplePeer::endOfRound(std::vector<Peer*>& peers) {
    logSentMessages(peers);
    applyChurnEvents(peers);
}

void ChurnExamplePeer::endOfExperiment(std::vector<Peer*>& peers) {
    int total = 0;
    for (Peer* peerPtr : peers) {
        if (auto* churnPeer = dynamic_cast<ChurnExamplePeer*>(peerPtr)) {
            total += churnPeer->msgsSent;
        }
    }
    OutputWriter::pushValue("finalMessageCount", total);
}

void ChurnExamplePeer::checkInStrm() {
    while (!inStreamEmpty()) {
        Packet packet = popInStream();
        logInboundMessage(packet);
    }
}

void ChurnExamplePeer::logInboundMessage(const Packet& packet) const {
    json payload = packet.getMessage();
    json logEntry;
    logEntry["to"] = publicId();
    logEntry["from"] = packet.sourceId();
    logEntry["receivedRound"] = RoundManager::currentRound();
    logEntry["contents"] = payload;
    OutputWriter::pushValue("receivedMessages", logEntry);
}

json ChurnExamplePeer::buildGreetingPayload() const {
    json payload;
    payload["type"] = "churn_greeting";
    payload["from"] = publicId();
    payload["roundSent"] = RoundManager::currentRound();
    payload["sequence"] = msgsSent;
    return payload;
}

void ChurnExamplePeer::applyInitialMembership(std::vector<Peer*>& peers) {
    if (_initialMembershipApplied) return;
    _initialMembershipApplied = true;

    for (interfaceId peerId : _initiallyAbsentIds) {
        disconnectPeerId(peers, peerId);
        if (Peer* localPeer = findLocalPeerById(peers, peerId)) {
            localPeer->setCrashRecoveryRound(std::numeric_limits<size_t>::max());
        }
        logMembershipEvent("initially_absent", peerId, findLocalPeerById(peers, peerId));
    }

    for (interfaceId peerId : _initiallyPresentIds) {
        reconnectPeerId(peers, peerId);
        if (Peer* localPeer = findLocalPeerById(peers, peerId)) {
            localPeer->setCrashRecoveryRound(0);
        }
    }
}

void ChurnExamplePeer::applyChurnEvents(std::vector<Peer*>& peers) {
    applyInitialMembership(peers);
    ++_step;

    for (auto& event : _events) {
        if (event.applied || _step < event.step) continue;
        if (event.action == "leave") {
            disconnectPeerId(peers, event.peerId);
            if (Peer* localPeer = findLocalPeerById(peers, event.peerId)) {
                localPeer->setCrashRecoveryRound(std::numeric_limits<size_t>::max());
            }
            logMembershipEvent("leave", event.peerId, findLocalPeerById(peers, event.peerId));
        } else if (event.action == "join") {
            reconnectPeerId(peers, event.peerId);
            if (Peer* localPeer = findLocalPeerById(peers, event.peerId)) {
                localPeer->setCrashRecoveryRound(0);
            }
            logMembershipEvent("join", event.peerId, findLocalPeerById(peers, event.peerId));
        }
        event.applied = true;
    }
}

Peer* ChurnExamplePeer::findLocalPeerById(std::vector<Peer*>& peers, interfaceId peerId) const {
    for (Peer* peerPtr : peers) {
        if (peerPtr && peerPtr->publicId() == peerId) {
            return peerPtr;
        }
    }
    return nullptr;
}

void ChurnExamplePeer::disconnectPeerId(std::vector<Peer*>& peers, interfaceId peerId) const {
    for (Peer* peerPtr : peers) {
        if (!peerPtr) continue;
        peerPtr->removeNeighbor(peerId);
        if (peerPtr->publicId() == peerId) {
            auto oldNeighbors = peerPtr->neighbors();
            for (interfaceId neighbor : oldNeighbors) {
                peerPtr->removeNeighbor(neighbor);
            }
            peerPtr->discardInbound();
        }
    }
}

void ChurnExamplePeer::reconnectPeerId(std::vector<Peer*>& peers, interfaceId peerId) const {
    Peer* target = findLocalPeerById(peers, peerId);
    for (Peer* peerPtr : peers) {
        if (!peerPtr || peerPtr->publicId() == peerId) continue;
        peerPtr->addNeighbor(peerId);
        if (target) {
            target->addNeighbor(peerPtr->publicId());
        }
    }
    if (target) {
        target->discardInbound();
    }
}

void ChurnExamplePeer::logMembershipEvent(const std::string& event, interfaceId peerId, const Peer* localPeer) const {
    json eventLog = {
        {"event", event},
        {"peerId", peerId},
        {"round", RoundManager::currentRound()},
        {"step", _step},
        {"local", localPeer != nullptr},
        {"neighbors", localPeer ? localPeer->neighbors().size() : 0}
    };
    OutputWriter::pushValue("membershipEvents", eventLog);
}

void ChurnExamplePeer::logSentMessages(const std::vector<Peer*>& peers) const {
    int total = 0;
    for (Peer* peerPtr : peers) {
        if (auto* churnPeer = dynamic_cast<ChurnExamplePeer*>(peerPtr)) {
            total += churnPeer->msgsSent;
        }
    }
    OutputWriter::pushValue("sentMessages", total);
}

} // namespace quantas
