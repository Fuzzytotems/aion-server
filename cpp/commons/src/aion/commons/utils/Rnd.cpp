#include "aion/commons/utils/Rnd.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <random>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"

namespace aion::commons::utils::Rnd {

namespace {

// messages of java.util.random.RandomSupport
constexpr const char* BAD_BOUND = "bound must be positive";
constexpr const char* BAD_FLOATING_BOUND = "bound must be finite and positive";
constexpr const char* BAD_RANGE = "bound must be greater than origin";

uint64_t splitMix64(uint64_t& x) noexcept {
	uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

uint64_t createSeed() noexcept {
	uint64_t seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
	seed ^= std::rotl(static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()), 32);
	try {
		std::random_device device;
		seed ^= (static_cast<uint64_t>(device()) << 32) | device();
	} catch (...) {
		// no entropy source available, the clocks have to do
	}
	return seed;
}

/** Java: Rnd.rnd's initializer - splits a generator for the calling thread off the shared one */
Xoshiro256PlusPlus splitSharedGenerator() {
	static std::mutex mutex;
	static Xoshiro256PlusPlus shared(createSeed());
	std::lock_guard lock(mutex);
	Xoshiro256PlusPlus result = shared;
	shared.jump();
	return result;
}

uint64_t nextU64() noexcept {
	return generator()();
}

/** @return a uniformly distributed value in [0, range), range must be positive */
uint64_t boundedNext(uint64_t range) noexcept {
	const uint64_t mask = range - 1;
	if ((range & mask) == 0)
		return nextU64() & mask;
	// reject the lowest (2^64 mod range) values, so that the remaining count is a multiple of range
	const uint64_t threshold = (uint64_t{0} - range) % range;
	while (true) {
		uint64_t r = nextU64();
		if (r >= threshold)
			return r % range;
	}
}

template <std::floating_point F>
F boundedNextFloating(F unit, F origin, F bound) noexcept {
	F r = unit;
	if (bound - origin < std::numeric_limits<F>::infinity()) {
		r = r * (bound - origin) + origin;
	} else { // avoids overflow at the cost of 3 more multiplications
		F halfOrigin = F(0.5) * origin;
		r = (r * (F(0.5) * bound - halfOrigin) + halfOrigin) * F(2);
	}
	if (r >= bound) // may need to correct a rounding problem
		r = std::nextafter(bound, -std::numeric_limits<F>::infinity());
	return r;
}

template <std::floating_point F>
void checkBound(F bound) {
	if (!(F(0) < bound && bound < std::numeric_limits<F>::infinity()))
		throw IllegalArgumentException(BAD_FLOATING_BOUND);
}

template <std::floating_point F>
void checkRange(F origin, F bound) {
	if (!(-std::numeric_limits<F>::infinity() < origin && origin < bound && bound < std::numeric_limits<F>::infinity()))
		throw IllegalArgumentException(BAD_RANGE);
}

} // namespace

Xoshiro256PlusPlus::Xoshiro256PlusPlus(uint64_t seed) noexcept {
	for (uint64_t& word : state)
		word = splitMix64(seed);
}

void Xoshiro256PlusPlus::jump() noexcept {
	static constexpr std::array<uint64_t, 4> JUMP = {0x180EC6D33CFD0ABAULL, 0xD5A61266F0C9392CULL, 0xA9582618E03FC9AAULL, 0x39ABDC4529B1661CULL};
	std::array<uint64_t, 4> jumped{};
	for (uint64_t jumpWord : JUMP) {
		for (int bit = 0; bit < 64; bit++) {
			if (jumpWord & (uint64_t{1} << bit)) {
				for (size_t i = 0; i < jumped.size(); i++)
					jumped[i] ^= state[i];
			}
			(*this)();
		}
	}
	state = jumped;
}

Xoshiro256PlusPlus& generator() noexcept {
	thread_local Xoshiro256PlusPlus engine = splitSharedGenerator();
	return engine;
}

void seedCurrentThreadForTests(uint64_t seed) noexcept {
	generator() = Xoshiro256PlusPlus(seed);
}

float chance() {
	return nextFloat(100.0f);
}

int32_t get(int32_t minInclusive, int32_t maxInclusive) {
	if (maxInclusive < minInclusive) {
		static const auto log = logging::LoggerFactory::getLogger("com.aionemu.commons.utils.Rnd");
		log.warn("", IllegalArgumentException("max < min"));
		maxInclusive = minInclusive;
	}
	return minInclusive == maxInclusive ? minInclusive : static_cast<int32_t>(nextLong(minInclusive, maxInclusive + int64_t{1}));
}

int32_t nextInt() {
	return static_cast<int32_t>(nextU64() >> 32);
}

int32_t nextInt(int32_t bound) {
	if (bound <= 0)
		throw IllegalArgumentException(BAD_BOUND);
	return static_cast<int32_t>(boundedNext(static_cast<uint64_t>(bound)));
}

int32_t nextInt(int32_t origin, int32_t bound) {
	if (origin >= bound)
		throw IllegalArgumentException(BAD_RANGE);
	const uint64_t range = static_cast<uint64_t>(int64_t{bound} - origin);
	return static_cast<int32_t>(origin + static_cast<int64_t>(boundedNext(range)));
}

int64_t nextLong() {
	return static_cast<int64_t>(nextU64());
}

int64_t nextLong(int64_t bound) {
	if (bound <= 0)
		throw IllegalArgumentException(BAD_BOUND);
	return static_cast<int64_t>(boundedNext(static_cast<uint64_t>(bound)));
}

int64_t nextLong(int64_t origin, int64_t bound) {
	if (origin >= bound)
		throw IllegalArgumentException(BAD_RANGE);
	const uint64_t range = static_cast<uint64_t>(bound) - static_cast<uint64_t>(origin); // wraps correctly for ranges > INT64_MAX
	return static_cast<int64_t>(static_cast<uint64_t>(origin) + boundedNext(range));
}

float nextFloat() {
	return static_cast<float>(nextU64() >> 40) * 0x1.0p-24f; // 24 random bits
}

float nextFloat(float bound) {
	checkBound(bound);
	float r = nextFloat() * bound;
	if (r >= bound) // may need to correct a rounding problem
		r = std::nextafter(bound, -std::numeric_limits<float>::infinity());
	return r;
}

float nextFloat(float origin, float bound) {
	checkRange(origin, bound);
	return boundedNextFloating(nextFloat(), origin, bound);
}

double nextDouble() {
	return static_cast<double>(nextU64() >> 11) * 0x1.0p-53; // 53 random bits
}

double nextDouble(double bound) {
	checkBound(bound);
	double r = nextDouble() * bound;
	if (r >= bound)
		r = std::nextafter(bound, -std::numeric_limits<double>::infinity());
	return r;
}

double nextDouble(double origin, double bound) {
	checkRange(origin, bound);
	return boundedNextFloating(nextDouble(), origin, bound);
}

bool nextBoolean() {
	return static_cast<int64_t>(nextU64()) < 0;
}

void nextBytes(std::span<uint8_t> bytes) {
	size_t i = 0;
	while (i < bytes.size()) {
		uint64_t random = nextU64();
		for (size_t n = std::min<size_t>(bytes.size() - i, 8); n-- > 0; random >>= 8)
			bytes[i++] = static_cast<uint8_t>(random);
	}
}

namespace detail {

size_t randomIndex(size_t size) {
	return static_cast<size_t>(boundedNext(size));
}

void throwEmpty(bool intElements) {
	throw IllegalArgumentException(intElements ? "Cannot get random int from an empty array." : "Cannot get random element from an empty list.");
}

} // namespace detail

} // namespace aion::commons::utils::Rnd
