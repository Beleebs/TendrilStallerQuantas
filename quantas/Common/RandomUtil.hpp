#ifndef RANDOM_UTIL_HPP
#define RANDOM_UTIL_HPP

#include <random>
#include <thread>
#include <ctime>
#include <stdexcept>
#include <string>
#include <atomic>
#include <cstdint>
#include <limits>
#include <functional>

namespace quantas {

namespace random_detail {

inline std::atomic<uint64_t>& seedGeneration() {
    static std::atomic<uint64_t> generation{1};
    return generation;
}

inline uint32_t mixSeed(uint32_t seed, uint32_t value) {
    seed ^= value + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
    seed ^= seed >> 16u;
    seed *= 0x7feb352du;
    seed ^= seed >> 15u;
    seed *= 0x846ca68bu;
    seed ^= seed >> 16u;
    return seed;
}

inline uint32_t initialRandomSeed() {
    std::random_device rd;
    const auto now = static_cast<uint32_t>(std::time(nullptr));
    return mixSeed(rd(), now);
}

inline std::atomic<uint32_t>& baseSeed() {
    static std::atomic<uint32_t> seed{initialRandomSeed()};
    return seed;
}

} // namespace random_detail

inline uint32_t randomSeed() {
    return random_detail::initialRandomSeed();
}

inline void setRandomSeed(uint32_t seed) {
    random_detail::baseSeed().store(seed, std::memory_order_relaxed);
    random_detail::seedGeneration().fetch_add(1, std::memory_order_relaxed);
}

//
// 1) A single thread_local function that gives us the engine
//
inline std::mt19937& threadLocalEngine() {
    static thread_local std::mt19937 engine;
    static thread_local uint64_t localGeneration = 0;

    const uint64_t currentGeneration = random_detail::seedGeneration().load(std::memory_order_relaxed);
    if (localGeneration != currentGeneration) {
        const uint32_t seed = random_detail::baseSeed().load(std::memory_order_relaxed);
        const auto threadSeed = static_cast<uint32_t>(
            std::hash<std::thread::id>{}(std::this_thread::get_id()) & std::numeric_limits<uint32_t>::max()
        );
        engine.seed(random_detail::mixSeed(seed, threadSeed));
        localGeneration = currentGeneration;
    }
    return engine;
}

//
// 2) Uniform integer in [min, max]
//
inline int uniformInt(int minVal, int maxVal) {
    if (minVal > maxVal) {
        throw std::invalid_argument("uniformInt: minVal > maxVal");
    }
    std::uniform_int_distribution<int> dist(minVal, maxVal);
    return dist(threadLocalEngine());
}

//
// 3) Uniform real in [min, max)
//
inline double uniformReal(double minVal, double maxVal) {
    if (minVal > maxVal) {
        throw std::invalid_argument("uniformReal: minVal > maxVal");
    }
    std::uniform_real_distribution<double> dist(minVal, maxVal);
    return dist(threadLocalEngine());
}

//
// 4) randMod(exclusiveMax) -> returns integer in [0, exclusiveMax-1]
//
inline int randMod(int exclusiveMax) {
    if (exclusiveMax <= 0) {
        throw std::invalid_argument(
            "randMod: exclusiveMax must be > 0, received: " + std::to_string(exclusiveMax)
        );
    }
    // Just reuse uniformInt
    return uniformInt(0, exclusiveMax - 1);
}

//
// 5) trueWithProbability(p) -> returns true with probability p
//
inline bool trueWithProbability(double p) {
    if (p <= 0.0) {
        return false;
    } else if (p >= 1.0) {
        return true;
    } else {
        return (uniformReal(0.0, 1.0) < p);
    }
}

inline int poissonInt(double lambda) {
    if (lambda <= 0.0) {
        throw std::invalid_argument(
            "poissonInt: mean (lambda) must be > 0, received: " + std::to_string(lambda)
        );
    }
    std::poisson_distribution<int> dist(lambda);
    return dist(threadLocalEngine());
}

} // namespace quantas

#endif // RANDOM_UTIL_HPP
