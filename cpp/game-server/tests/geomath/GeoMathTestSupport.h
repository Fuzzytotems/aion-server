#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>

#include <gtest/gtest.h>

#include "aion/gameserver/geoEngine/math/Matrix3f.h"
#include "aion/gameserver/geoEngine/math/Matrix4f.h"
#include "aion/gameserver/geoEngine/math/Vector2f.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"

namespace aion::gameserver::geoEngine::math::test {

inline float fl(uint32_t bits) {
	return std::bit_cast<float>(bits);
}

inline double db(uint64_t bits) {
	return std::bit_cast<double>(bits);
}

inline uint32_t bitsOf(float value) {
	return std::bit_cast<uint32_t>(value);
}

inline uint64_t bitsOf(double value) {
	return std::bit_cast<uint64_t>(value);
}

/** Bit-exact float comparison; any two NaNs match (NaN payloads of hardware-generated NaNs are not part of the Java semantics). */
inline ::testing::AssertionResult sameFloat(float actual, float expected) {
	if ((actual != actual && expected != expected) || bitsOf(actual) == bitsOf(expected))
		return ::testing::AssertionSuccess();
	return ::testing::AssertionFailure() << std::format("actual {} (0x{:08x}) != expected {} (0x{:08x})", actual, bitsOf(actual), expected,
	                                                    bitsOf(expected));
}

inline ::testing::AssertionResult sameDouble(double actual, double expected) {
	if ((actual != actual && expected != expected) || bitsOf(actual) == bitsOf(expected))
		return ::testing::AssertionSuccess();
	return ::testing::AssertionFailure() << std::format("actual {} (0x{:016x}) != expected {} (0x{:016x})", actual, bitsOf(actual), expected,
	                                                    bitsOf(expected));
}

#define EXPECT_FLOAT_BITS(actual, expected) EXPECT_TRUE(::aion::gameserver::geoEngine::math::test::sameFloat((actual), (expected)))
#define EXPECT_DOUBLE_BITS(actual, expected) EXPECT_TRUE(::aion::gameserver::geoEngine::math::test::sameDouble((actual), (expected)))

inline ::testing::AssertionResult sameVector(const Vector3f& actual, float x, float y, float z) {
	if (sameFloat(actual.x, x) && sameFloat(actual.y, y) && sameFloat(actual.z, z))
		return ::testing::AssertionSuccess();
	return ::testing::AssertionFailure() << std::format(
	         "actual ({}, {}, {}) [0x{:08x} 0x{:08x} 0x{:08x}] != expected ({}, {}, {}) [0x{:08x} 0x{:08x} 0x{:08x}]", actual.x, actual.y, actual.z,
	         bitsOf(actual.x), bitsOf(actual.y), bitsOf(actual.z), x, y, z, bitsOf(x), bitsOf(y), bitsOf(z));
}

inline ::testing::AssertionResult sameVector(const Vector3f& actual, const uint32_t expected[3]) {
	return sameVector(actual, fl(expected[0]), fl(expected[1]), fl(expected[2]));
}

inline ::testing::AssertionResult sameVector(const Vector2f& actual, const uint32_t expected[2]) {
	if (sameFloat(actual.x, fl(expected[0])) && sameFloat(actual.y, fl(expected[1])))
		return ::testing::AssertionSuccess();
	return ::testing::AssertionFailure() << std::format("actual ({}, {}) != expected ({}, {})", actual.x, actual.y, fl(expected[0]), fl(expected[1]));
}

inline Vector3f vec3(const uint32_t bits[3]) {
	return Vector3f(fl(bits[0]), fl(bits[1]), fl(bits[2]));
}

inline Vector2f vec2(const uint32_t bits[2]) {
	return Vector2f(fl(bits[0]), fl(bits[1]));
}

inline Matrix3f mat3(const uint32_t bits[9]) {
	std::array<float, 9> values{};
	for (size_t i = 0; i < values.size(); i++)
		values[i] = fl(bits[i]);
	Matrix3f m;
	m.set(values, true);
	return m;
}

inline Matrix4f mat4(const uint32_t bits[16]) {
	std::array<float, 16> values{};
	for (size_t i = 0; i < values.size(); i++)
		values[i] = fl(bits[i]);
	Matrix4f m;
	m.set(values, true);
	return m;
}

inline ::testing::AssertionResult sameMatrix(const Matrix3f& actual, const uint32_t expected[9]) {
	std::array<float, 9> values{};
	actual.get(values, true);
	for (size_t i = 0; i < values.size(); i++) {
		auto r = sameFloat(values[i], fl(expected[i]));
		if (!r)
			return ::testing::AssertionFailure() << "element " << i << ": " << r.message() << "\n" << actual.toString();
	}
	return ::testing::AssertionSuccess();
}

inline ::testing::AssertionResult sameMatrix(const Matrix4f& actual, const uint32_t expected[16]) {
	std::array<float, 16> values{};
	actual.get(values, true);
	for (size_t i = 0; i < values.size(); i++) {
		auto r = sameFloat(values[i], fl(expected[i]));
		if (!r)
			return ::testing::AssertionFailure() << "element " << i << ": " << r.message() << "\n" << actual.toString();
	}
	return ::testing::AssertionSuccess();
}

inline ::testing::AssertionResult sameMatrix(const Matrix4f& actual, const std::array<float, 16>& expected) {
	std::array<uint32_t, 16> bits{};
	for (size_t i = 0; i < bits.size(); i++)
		bits[i] = bitsOf(expected[i]);
	return sameMatrix(actual, bits.data());
}

} // namespace aion::gameserver::geoEngine::math::test
