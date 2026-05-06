/*
Copyright 2024

This file is part of QUANTAS.
QUANTAS is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
QUANTAS is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with QUANTAS. If not, see <https://www.gnu.org/licenses/>.
*/

#include "ChordPeer.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "../Common/Abstract/NetworkInterfaceAbstract.hpp"
#include "../Common/Concrete/NetworkInterfaceConcrete.hpp"
#include "../Common/Json.hpp"
#include "../Common/OutputWriter.hpp"
#include "../Common/RandomUtil.hpp"
#include "../Common/RoundManager.hpp"

namespace quantas {

namespace {

bool isClockwiseBetween(interfaceId self, interfaceId target, interfaceId candidate) {
    if (candidate == self || candidate == target) {
        return candidate == target;
    }

    if (target > self) {
        return candidate > self && candidate <= target;
    }
    return candidate > self || candidate <= target;
}

bool isFurtherClockwise(interfaceId self, interfaceId target, interfaceId candidate, interfaceId currentBest) {
    if (currentBest == NO_PEER_ID) {
        return true;
    }

    if (target > self) {
        return candidate > currentBest;
    }

    const bool candidateWrapped = candidate <= target;
    const bool bestWrapped = currentBest <= target;
    if (candidateWrapped != bestWrapped) {
        return candidateWrapped;
    }
    return candidate > currentBest;
}

} // namespace

static bool registerChordPeer = []() {
    return PeerRegistry::registerPeerType(
        "ChordPeer",
        [](interfaceId pubId) { return new ChordPeer(new NetworkInterfaceAbstract(pubId)); });
}();

static bool registerChordConcretePeer = []() {
    return PeerRegistry::registerPeerType(
        "ChordPeerConcrete",
        [](interfaceId /*pubId*/) { return new ChordPeer(new NetworkInterfaceConcrete()); });
}();

int ChordPeer::s_nextTransactionId = 1;

ChordPeer::ChordPeer(NetworkInterface* interfacePtr)
    : Peer(interfacePtr) {}

ChordPeer::ChordPeer(const ChordPeer& rhs)
    : Peer(rhs),
      _fingers(rhs._fingers),
      _networkSize(rhs._networkSize),
      _initialized(rhs._initialized),
      _requestsSatisfied(rhs._requestsSatisfied),
      _totalHops(rhs._totalHops),
      _totalLatency(rhs._totalLatency),
      _maxHops(rhs._maxHops),
      _stopAfterSatisfiedRequests(rhs._stopAfterSatisfiedRequests),
      _stopRequested(rhs._stopRequested) {}

void ChordPeer::initParameters(const std::vector<Peer*>& peers, json parameters) {
    s_nextTransactionId = 1;
    int configuredNetworkSize = static_cast<int>(peers.size());
    int stopAfterSatisfiedRequests = -1;
    if (parameters.is_object()) {
        configuredNetworkSize = std::max(1,parameters.value("initialPeers", 1));
        stopAfterSatisfiedRequests = parameters.value("stopAfterSatisfiedRequests", -1);
    }

    for (auto* basePtr : peers) {
        auto* peerPtr = static_cast<ChordPeer*>(basePtr);
        peerPtr->_networkSize = configuredNetworkSize;
        peerPtr->_requestsSatisfied = 0;
        peerPtr->_totalHops = 0;
        peerPtr->_totalLatency = 0;
        peerPtr->_maxHops = 0;
        peerPtr->_stopAfterSatisfiedRequests = stopAfterSatisfiedRequests;
        peerPtr->_stopRequested = false;
        peerPtr->_initialized = true;
        peerPtr->buildFingerTable();
    }
}

void ChordPeer::performComputation() {
    if (!_initialized) return;
    checkInStrm();
}

void ChordPeer::checkInStrm() {
    while (!inStreamEmpty()) {
        Packet packet = popInStream();
        json msg = packet.getMessage();
        if (!msg.contains("type") || msg["type"] != "Chord") continue;
        const std::string messageType = msg.value("messageType", std::string());
        if (messageType == "lookup") {
            handleLookup(std::move(msg));
        }
    }
}

void ChordPeer::handleLookup(json msg) {
    interfaceId target = msg.value("targetId", NO_PEER_ID);
    if (target == NO_PEER_ID) return;

    if (target == publicId()) {
        ++_requestsSatisfied;
        const int hops = msg.value("hops", 0);
        _totalHops += hops;
        _maxHops = std::max(_maxHops, hops);
        if (!_stopRequested &&
            _stopAfterSatisfiedRequests > 0 &&
            _requestsSatisfied >= _stopAfterSatisfiedRequests) {
            _stopRequested = true;
            requestSimulationStop("ChordSatisfiedRequestThreshold");
        }
        int submitted = msg.value("roundSubmitted", static_cast<int>(RoundManager::currentRound()));
        _totalLatency += static_cast<int>(RoundManager::currentRound()) - submitted;
        return;
    }

    const std::set<interfaceId> neighborSet = neighbors();
    interfaceId nextHop = selectFinger(target, neighborSet);
    if (nextHop == NO_PEER_ID) {
        nextHop = chooseClockwiseNeighbor(target, neighborSet);
    }
    if (nextHop == NO_PEER_ID) return;

    dispatchLookup(std::move(msg), nextHop, neighborSet);
}

void ChordPeer::submitLookup(int transactionId) {
    if (!_initialized) return;

    interfaceId target = pickRandomTarget();
    json msg = makeLookupTemplate(target, transactionId);
    if (target == publicId()) {
        ++_requestsSatisfied;
        return;
    }

    const std::set<interfaceId> neighborSet = neighbors();
    interfaceId nextHop = selectFinger(target, neighborSet);
    if (nextHop == NO_PEER_ID) {
        nextHop = chooseClockwiseNeighbor(target, neighborSet);
    }
    if (nextHop == NO_PEER_ID) return;

    dispatchLookup(std::move(msg), nextHop, neighborSet);
}

json ChordPeer::makeLookupTemplate(interfaceId target, int transactionId) const {
    json msg = {
        {"type", "Chord"},
        {"messageType", "lookup"},
        {"transactionId", transactionId},
        {"originId", publicId()},
        {"targetId", target},
        {"roundSubmitted", static_cast<int>(RoundManager::currentRound())},
        {"hops", 0}
    };
    return msg;
}

interfaceId ChordPeer::pickRandomTarget() const {
    const int upperBound = _networkSize;
    if (upperBound <= 1) return publicId();

    interfaceId candidate = static_cast<interfaceId>(randMod(upperBound));
    if (candidate == publicId()) {
        candidate = static_cast<interfaceId>((candidate + 1) % upperBound);
    }
    return candidate;
}

interfaceId ChordPeer::selectFinger(interfaceId target, const std::set<interfaceId>& neighborSet) const {
    interfaceId best = NO_PEER_ID;

    for (const auto& entry : _fingers) {
        if (!neighborSet.count(entry.nodeId)) continue;
        if (!isClockwiseBetween(publicId(), target, entry.nodeId)) continue;
        if (isFurtherClockwise(publicId(), target, entry.nodeId, best)) {
            best = entry.nodeId;
        }
    }

    return best;
}

interfaceId ChordPeer::chooseClockwiseNeighbor(interfaceId target,
                                                        const std::set<interfaceId>& neighborSet) const {
    (void)target;
    if (neighborSet.empty()) return NO_PEER_ID;

    interfaceId best = NO_PEER_ID;

    for (interfaceId neighbor : neighborSet) {
        if (neighbor == publicId()) continue;
        if (neighbor > publicId()) {
            if (best == NO_PEER_ID || best <= publicId() || neighbor < best) {
                best = neighbor;
            }
        } else if (best == NO_PEER_ID || best <= publicId() && neighbor < best) {
            best = neighbor;
        }
    }
    return best;
}

void ChordPeer::dispatchLookup(json msg,
                                        interfaceId nextHop,
                                        const std::set<interfaceId>& neighborSet) {
    if (nextHop == NO_PEER_ID || nextHop == publicId()) return;
    if (!neighborSet.count(nextHop)) return;
    msg["hops"] = msg.value("hops", 0) + 1;
    msg["lastHop"] = publicId();
    unicastTo(msg, nextHop);
}

void ChordPeer::buildFingerTable() {
    _fingers.clear();

    for (interfaceId neighbor : neighbors()) {
        if (neighbor == publicId()) continue;
        _fingers.push_back(FingerEntry{neighbor});
    }

    std::sort(_fingers.begin(), _fingers.end(),
              [](const FingerEntry& lhs, const FingerEntry& rhs) { return lhs.nodeId < rhs.nodeId; });
    _fingers.erase(std::unique(_fingers.begin(), _fingers.end(),
                               [](const FingerEntry& lhs, const FingerEntry& rhs) {
                                   return lhs.nodeId == rhs.nodeId;
                               }),
                   _fingers.end());
}

void ChordPeer::endOfRound(std::vector<Peer*>& peers) {
    if (peers.empty()) return;
    for (auto* basePtr : peers) {
        auto* peerPtr = static_cast<ChordPeer*>(basePtr);
        if (randMod(static_cast<int>(peerPtr->_networkSize)))
            peerPtr->submitLookup(s_nextTransactionId++);        
    }
}

void ChordPeer::endOfExperiment(std::vector<Peer*>& peers) {
    if (peers.empty()) return;

    long long totalSatisfied = 0;
    long long totalHops = 0;
    long long totalLatency = 0;
    int maxHops = 0;
    for (auto* basePtr : peers) {
        auto* peerPtr = static_cast<ChordPeer*>(basePtr);
        maxHops = std::max(maxHops, peerPtr->_maxHops);
        totalSatisfied += peerPtr->_requestsSatisfied;
        totalHops += peerPtr->_totalHops;
        totalLatency += peerPtr->_totalLatency;
    }

    if (totalSatisfied > 0) {
        OutputWriter::pushValue("ChordAverageHops", static_cast<double>(totalHops) / totalSatisfied);
        OutputWriter::pushValue("ChordAverageLatency", static_cast<double>(totalLatency) / totalSatisfied);
    }
    OutputWriter::pushValue("ChordRequestsSatisfied", static_cast<double>(totalSatisfied));
    

    OutputWriter::pushValue("ChordMaxHops", static_cast<double>(maxHops));
}

} // namespace quantas
