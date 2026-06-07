/*
Copyright 2024

This file is part of QUANTAS.
QUANTAS is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. QUANTAS is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
QUANTAS. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef POWETHEREUM_HPP
#define POWETHEREUM_HPP

#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "../Common/Pow.hpp"

namespace quantas {

// Ethereum-specific PoW ledger that applies a simplified GHOST rule: the
// preferred head is the one whose subtree (including itself) contains the most
// blocks. Heights are used only as a tie breaker to keep behaviour stable.
class PoWEthereum : public PoW {
public:
    explicit PoWEthereum(Committee* committee)
        : PoW(committee) {}
    ~PoWEthereum() override = default;

protected:
    bool preferCandidate(const BlockRecord& candidate,
                         const BlockRecord* incumbent) const override {
        if (!incumbent) return true;
        if (isAncestor(incumbent->hash, candidate.hash)) {
            return true;
        }
        if (isAncestor(candidate.hash, incumbent->hash)) {
            return false;
        }

        const int candidateWeight = subtreeSize(candidate.hash);
        const int incumbentWeight = subtreeSize(incumbent->hash);
        if (candidateWeight != incumbentWeight) {
            return candidateWeight > incumbentWeight;
        }
        if (candidate.height != incumbent->height) {
            return candidate.height > incumbent->height;
        }
        return candidate.hash < incumbent->hash;
    }

    std::string selectBestHash() const override {
        std::unordered_map<std::string, int> weightMemo;
        std::unordered_set<std::string> active;
        std::function<int(const std::string&)> subtreeWeight = [&](const std::string& hash) -> int {
            auto memoIt = weightMemo.find(hash);
            if (memoIt != weightMemo.end()) return memoIt->second;
            if (!active.insert(hash).second) return 0;

            int weight = _blocks.count(hash) ? 1 : 0;
            auto childIt = _children.find(hash);
            if (childIt != _children.end()) {
                for (const auto& child : childIt->second) {
                    if (_blocks.count(child)) {
                        weight += subtreeWeight(child);
                    }
                }
            }

            active.erase(hash);
            weightMemo[hash] = weight;
            return weight;
        };

        std::string cursor = "GENESIS";
        while (true) {
            auto childIt = _children.find(cursor);
            if (childIt == _children.end() || childIt->second.empty()) {
                return cursor;
            }

            std::string bestChild;
            int bestWeight = -1;
            int bestHeight = -1;
            for (const auto& child : childIt->second) {
                auto blockIt = _blocks.find(child);
                if (blockIt == _blocks.end() || !hasFullAncestry(child)) continue;

                const int weight = subtreeWeight(child);
                const int height = blockIt->second.height;
                if (bestChild.empty() ||
                    weight > bestWeight ||
                    (weight == bestWeight && height > bestHeight) ||
                    (weight == bestWeight && height == bestHeight && child < bestChild)) {
                    bestChild = child;
                    bestWeight = weight;
                    bestHeight = height;
                }
            }

            if (bestChild.empty()) {
                return cursor;
            }
            cursor = bestChild;
        }
    }
};

}

#endif // POWETHEREUM_HPP
