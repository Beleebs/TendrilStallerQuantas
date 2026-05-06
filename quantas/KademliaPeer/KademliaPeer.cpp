/*
Copyright 2024

This file is part of QUANTAS.
QUANTAS is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
QUANTAS is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with QUANTAS. If not, see <https://www.gnu.org/licenses/>.
*/

#include "KademliaPeer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../Common/Abstract/NetworkInterfaceAbstract.hpp"
#include "../Common/Concrete/NetworkInterfaceConcrete.hpp"
#include "../Common/OutputWriter.hpp"
#include "../Common/RandomUtil.hpp"
#include "../Common/RoundManager.hpp"

namespace quantas {

namespace {
std::uint64_t xorDistance(interfaceId lhs, interfaceId rhs) {
    return static_cast<std::uint64_t>(static_cast<std::uint64_t>(lhs) ^ static_cast<std::uint64_t>(rhs));
}
}  // namespace

static bool registerKademliaAbstract = []() {
    return PeerRegistry::registerPeerType(
        "KademliaPeer",
        [](interfaceId pubId) { return new KademliaPeer(new NetworkInterfaceAbstract(pubId)); });
}();

static bool registerKademliaConcrete = []() {
    return PeerRegistry::registerPeerType(
        "KademliaPeerConcrete",
        [](interfaceId /*pubId*/) { return new KademliaPeer(new NetworkInterfaceConcrete()); });
}();

int KademliaPeer::s_currentTransactionId = 1;

KademliaPeer::~KademliaPeer() = default;

KademliaPeer::KademliaPeer(const KademliaPeer& rhs)
    : Peer(rhs),
      _binaryIdSize(rhs._binaryIdSize),
      _networkSize(rhs._networkSize),
      _binaryId(rhs._binaryId),
      _fingers(rhs._fingers),
      _lastNeighborFingerprint(rhs._lastNeighborFingerprint),
      _requestsSatisfied(rhs._requestsSatisfied),
      _totalHops(rhs._totalHops),
      _maxHops(rhs._maxHops),
      _latency(rhs._latency),
      _stopAfterSatisfiedRequests(rhs._stopAfterSatisfiedRequests),
      _stopRequested(rhs._stopRequested),
      _alive(rhs._alive),
      _initialized(rhs._initialized) {}

KademliaPeer::KademliaPeer(NetworkInterface* networkInterface)
    : Peer(networkInterface) {}

void KademliaPeer::initParameters(const std::vector<Peer*>& peers, json parameters) {
    s_currentTransactionId = 1;
    int configuredNetworkSize = static_cast<int>(peers.size());
    int stopAfterSatisfiedRequests = -1;
    if (parameters.is_object()) {
        configuredNetworkSize = std::max(1, parameters.value("initialPeers", configuredNetworkSize));
        stopAfterSatisfiedRequests = parameters.value("stopAfterSatisfiedRequests", -1);
    }

    for (auto* base : peers) {
        auto* peer = static_cast<KademliaPeer*>(base);
        peer->_networkSize = std::max(1, configuredNetworkSize);
        const int upperBound = peer->_networkSize;
        peer->_binaryIdSize = 1;
        while ((static_cast<std::uint64_t>(1) << peer->_binaryIdSize) < static_cast<std::uint64_t>(upperBound) &&
               peer->_binaryIdSize < static_cast<int>(sizeof(std::uint64_t) * 8 - 1)) {
            ++peer->_binaryIdSize;
        }
        peer->_binaryId = peer->getBinaryId(peer->publicId());
        peer->_initialized = true;
        peer->_requestsSatisfied = 0;
        peer->_totalHops = 0;
        peer->_maxHops = 0;
        peer->_latency = 0;
        peer->_stopAfterSatisfiedRequests = stopAfterSatisfiedRequests;
        peer->_stopRequested = false;
        peer->_fingers.clear();
        peer->_lastNeighborFingerprint = 0;
        peer->rebuildFingerTable(peer->neighbors());
    }
}

void KademliaPeer::performComputation() {
    if (!_alive) return;
    if (!_initialized) return;

    const std::set<interfaceId> neighborSet = neighbors();
    const size_t fingerprint = neighborFingerprint(neighborSet);
    if (_fingers.empty() || fingerprint != _lastNeighborFingerprint) {
        rebuildFingerTable(neighborSet);
    }

    checkInStrm();
}

void KademliaPeer::checkInStrm() {
    while (!inStreamEmpty()) {
        Packet packet = popInStream();
        json message = packet.getMessage();
        if (!message.is_object()) continue;
        if (message.value("type", std::string()) != "Kademlia") continue;
        if (message.value("messageType", std::string()) != "lookup") continue;
        handleLookup(std::move(message));
    }
}

void KademliaPeer::handleLookup(json msg) {
    interfaceId targetId = msg.value("targetId", NO_PEER_ID);
    if (targetId == NO_PEER_ID) return;

    if (targetId == publicId()) {
        ++_requestsSatisfied;
        const int hops = msg.value("hops", 0);
        _totalHops += hops;
        _maxHops = std::max(_maxHops, hops);
        if (!_stopRequested &&
            _stopAfterSatisfiedRequests > 0 &&
            _requestsSatisfied >= _stopAfterSatisfiedRequests) {
            _stopRequested = true;
            requestSimulationStop("KademliaSatisfiedRequestThreshold");
        }
        int submitted = msg.value("roundSubmitted", static_cast<int>(RoundManager::currentRound()));
        _latency += static_cast<int>(RoundManager::currentRound()) - submitted;
        return;
    }

    const std::set<interfaceId> neighborSet = neighbors();
    if (neighborSet.empty()) return;

    std::string targetBinary = msg.value("targetBinaryId", std::string());
    if (targetBinary.empty()) {
        targetBinary = getBinaryId(targetId);
        msg["targetBinaryId"] = targetBinary;
    }

    interfaceId nextHop = findRoute(targetBinary, targetId, neighborSet);
    if (nextHop == NO_PEER_ID || nextHop == publicId()) {
        return;
    }

    msg["hops"] = msg.value("hops", 0) + 1;
    msg["lastHop"] = publicId();
    unicastTo(std::move(msg), nextHop);
}

void KademliaPeer::submitLookup(int transactionId) {
    if (!_initialized) return;

    const int upperBound = _networkSize;
    if (upperBound <= 1) {
        return;
    }

    interfaceId targetId = static_cast<interfaceId>(randMod(upperBound));
    if (targetId == publicId()) {
        targetId = static_cast<interfaceId>((targetId + 1) % upperBound);
    }

    std::string targetBinary = getBinaryId(targetId);
    json msg = makeLookupMessage(targetId, targetBinary, transactionId);

    if (targetId == publicId()) {
        ++_requestsSatisfied;
        return;
    }

    const std::set<interfaceId> neighborSet = neighbors();
    if (neighborSet.empty()) return;

    interfaceId nextHop = findRoute(targetBinary, targetId, neighborSet);
    if (nextHop == NO_PEER_ID || nextHop == publicId()) return;

    msg["hops"] = msg.value("hops", 0) + 1;
    msg["lastHop"] = publicId();
    unicastTo(std::move(msg), nextHop);
}

json KademliaPeer::makeLookupMessage(interfaceId targetId,
                                     const std::string& targetBinaryId,
                                     int transactionId) const {
    json msg = {
        {"type", "Kademlia"},
        {"messageType", "lookup"},
        {"transactionId", transactionId},
        {"originId", publicId()},
        {"targetId", targetId},
        {"targetBinaryId", targetBinaryId},
        {"roundSubmitted", static_cast<int>(RoundManager::currentRound())},
        {"hops", 0}
    };
    return msg;
}

std::string KademliaPeer::getBinaryId(interfaceId id) const {
    if (_binaryIdSize <= 0) return std::string();
    std::string result;
    result.reserve(static_cast<size_t>(_binaryIdSize));
    std::uint64_t value = static_cast<std::uint64_t>(id);
    for (int bit = _binaryIdSize - 1; bit >= 0; --bit) {
        if (bit >= static_cast<int>(sizeof(std::uint64_t) * 8)) {
            result.push_back('0');
            continue;
        }
        std::uint64_t mask = static_cast<std::uint64_t>(1) << bit;
        result.push_back((value & mask) ? '1' : '0');
    }
    return result;
}

interfaceId KademliaPeer::findRoute(const std::string& targetBinaryId,
                                    interfaceId targetId,
                                    const std::set<interfaceId>& neighborSet) const {
    if (targetBinaryId.empty() || _binaryId.empty()) {
        return selectClosestByDistance(targetId, neighborSet);
    }

    int group = firstDifferentBit(targetBinaryId, _binaryId);
    if (group < 0) {
        return selectClosestByDistance(targetId, neighborSet);
    }

    interfaceId finger = selectFingerForGroup(group, neighborSet);
    if (finger != NO_PEER_ID) {
        return finger;
    }

    return selectClosestByDistance(targetId, neighborSet);
}

interfaceId KademliaPeer::selectFingerForGroup(int group,
                                               const std::set<interfaceId>& neighborSet) const {
    std::vector<interfaceId> candidates;
    for (const auto& finger : _fingers) {
        if (finger.group != group) continue;
        if (!neighborSet.count(finger.Id)) continue;
        candidates.push_back(finger.Id);
    }

    if (candidates.empty()) return NO_PEER_ID;
    if (candidates.size() == 1) return candidates.front();

    int index = randMod(static_cast<int>(candidates.size()));
    return candidates[static_cast<size_t>(index)];
}

interfaceId KademliaPeer::selectClosestByDistance(interfaceId targetId,
                                                  const std::set<interfaceId>& neighborSet) const {
    std::uint64_t selfDistance = xorDistance(publicId(), targetId);
    std::uint64_t bestDistance = selfDistance;
    interfaceId best = NO_PEER_ID;

    for (interfaceId neighbor : neighborSet) {
        if (neighbor == publicId()) continue;
        std::uint64_t distance = xorDistance(neighbor, targetId);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = neighbor;
        }
    }

    return best;
}

int KademliaPeer::firstDifferentBit(const std::string& lhs, const std::string& rhs) {
    const size_t limit = std::min(lhs.size(), rhs.size());
    for (size_t idx = 0; idx < limit; ++idx) {
        if (lhs[idx] != rhs[idx]) {
            return static_cast<int>(idx);
        }
    }
    return -1;
}

void KademliaPeer::rebuildFingerTable(const std::set<interfaceId>& neighborSet) {
    _fingers.clear();
    if (_binaryIdSize <= 0) {
        _lastNeighborFingerprint = neighborFingerprint(neighborSet);
        return;
    }

    std::vector<std::vector<KademliaFinger>> grouped(static_cast<size_t>(_binaryIdSize));

    for (interfaceId neighbor : neighborSet) {
        if (neighbor == publicId()) continue;
        KademliaFinger entry;
        entry.Id = neighbor;
        entry.binId = getBinaryId(neighbor);
        entry.group = firstDifferentBit(entry.binId, _binaryId);
        if (entry.group >= 0) {
            grouped[static_cast<size_t>(entry.group)].push_back(std::move(entry));
        }
    }

    for (auto& bucket : grouped) {
        if (bucket.empty()) continue;
        size_t chosen = 0;
        if (bucket.size() > 1) {
            chosen = static_cast<size_t>(randMod(static_cast<int>(bucket.size())));
        }
        _fingers.push_back(bucket[chosen]);
    }

    _lastNeighborFingerprint = neighborFingerprint(neighborSet);
}

size_t KademliaPeer::neighborFingerprint(const std::set<interfaceId>& neighborSet) const {
    size_t hash = neighborSet.size();
    const size_t magic = static_cast<size_t>(0x9e3779b97f4a7c15ULL);
    for (interfaceId id : neighborSet) {
        hash ^= static_cast<size_t>(id) + magic + (hash << 6) + (hash >> 2);
    }
    return hash;
}

void KademliaPeer::endOfRound(std::vector<Peer*>& peers) {
    if (peers.empty()) return;

    std::vector<KademliaPeer*> typed;
    typed.reserve(peers.size());
    for (auto* base : peers) {
        typed.push_back(static_cast<KademliaPeer*>(base));
    }

    for (auto* peer : typed) {
        if (randMod(std::max(1, peer->_networkSize))) {
            peer->submitLookup(s_currentTransactionId++);
        }
    }
}

void KademliaPeer::endOfExperiment(std::vector<Peer*>& peers) {
    if (peers.empty()) return;

    long long satisfied = 0;
    long long hops = 0;
    long long latency = 0;
    int maxHops = 0;

    for (auto* base : peers) {
        auto* peer = static_cast<KademliaPeer*>(base);
        satisfied += peer->_requestsSatisfied;
        hops += peer->_totalHops;
        latency += peer->_latency;
        maxHops = std::max(maxHops, peer->_maxHops);
    }

    if (satisfied > 0) {
        const double avgHops = static_cast<double>(hops) / satisfied;
        OutputWriter::pushValue("kademliaAverageHops", avgHops);
        OutputWriter::pushValue("kademliaAverageLatency", static_cast<double>(latency) / satisfied);
    }
    OutputWriter::pushValue("kademliaRequestsSatisfied", static_cast<double>(satisfied));
    OutputWriter::pushValue("kademliaMaxHops", static_cast<double>(maxHops));
}

}  // namespace quantas
