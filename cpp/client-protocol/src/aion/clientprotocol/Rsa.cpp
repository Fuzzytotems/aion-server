#include "aion/clientprotocol/Rsa.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace aion::clientprotocol::rsa {

namespace {

// A number below 2^1056 as 33 little endian 32-bit limbs: one limb more than 1024 bits, so that 2 * r and r + a (both below 2 * modulus)
// never overflow before the reduction.
constexpr size_t LIMBS = BLOCK_SIZE / 4 + 1;
using Number = std::array<uint32_t, LIMBS>;

Number fromBytes(std::span<const uint8_t> bytes, const char* what) {
	size_t first = 0;
	while (first < bytes.size() && bytes[first] == 0)
		first++;
	if (bytes.size() - first > BLOCK_SIZE)
		throw std::invalid_argument(std::string(what) + " is longer than 128 bytes");
	Number n{};
	for (size_t i = 0; i < bytes.size() - first; i++) {
		const uint8_t b = bytes[bytes.size() - 1 - i];
		n[i / 4] |= static_cast<uint32_t>(b) << (8 * (i % 4));
	}
	return n;
}

std::array<uint8_t, BLOCK_SIZE> toBytes(const Number& n) {
	std::array<uint8_t, BLOCK_SIZE> bytes{};
	for (size_t i = 0; i < BLOCK_SIZE; i++)
		bytes[BLOCK_SIZE - 1 - i] = static_cast<uint8_t>(n[i / 4] >> (8 * (i % 4)));
	return bytes;
}

bool isZero(const Number& n) noexcept {
	for (uint32_t limb : n) {
		if (limb != 0)
			return false;
	}
	return true;
}

/** a < b */
bool less(const Number& a, const Number& b) noexcept {
	for (size_t i = LIMBS; i-- > 0;) {
		if (a[i] != b[i])
			return a[i] < b[i];
	}
	return false;
}

/** a += b (no overflow: both stay below 2^1056) */
void add(Number& a, const Number& b) noexcept {
	uint64_t carry = 0;
	for (size_t i = 0; i < LIMBS; i++) {
		const uint64_t sum = static_cast<uint64_t>(a[i]) + b[i] + carry;
		a[i] = static_cast<uint32_t>(sum);
		carry = sum >> 32;
	}
}

/** a -= b, for a >= b */
void subtract(Number& a, const Number& b) noexcept {
	uint64_t borrow = 0;
	for (size_t i = 0; i < LIMBS; i++) {
		const uint64_t difference = static_cast<uint64_t>(a[i]) - b[i] - borrow;
		a[i] = static_cast<uint32_t>(difference);
		borrow = difference >> 63;
	}
}

/** a = a + a - (modulus if that is not below it), for a < modulus */
void doubleMod(Number& a, const Number& modulus) noexcept {
	const Number copy = a;
	add(a, copy);
	if (!less(a, modulus))
		subtract(a, modulus);
}

/** a = a + b mod modulus, for a, b < modulus */
void addMod(Number& a, const Number& b, const Number& modulus) noexcept {
	add(a, b);
	if (!less(a, modulus))
		subtract(a, modulus);
}

/** a * b mod modulus by double and add over the bits of b (from the top), for a, b < modulus */
Number multiplyMod(const Number& a, const Number& b, const Number& modulus) noexcept {
	Number result{};
	for (size_t bit = LIMBS * 32; bit-- > 0;) {
		doubleMod(result, modulus);
		if (b[bit / 32] >> (bit % 32) & 1)
			addMod(result, a, modulus);
	}
	return result;
}

} // namespace

std::array<uint8_t, BLOCK_SIZE> modPow(std::span<const uint8_t> base, uint32_t exponent, std::span<const uint8_t> modulus) {
	const Number n = fromBytes(modulus, "Modulus");
	if (isZero(n))
		throw std::invalid_argument("Modulus is 0");
	const Number b = fromBytes(base, "RSA block");
	if (!less(b, n))
		throw std::invalid_argument("RSA block is not smaller than the modulus");

	// left-to-right square and multiply; 1 mod n is 0 for n == 1
	Number result{};
	result[0] = 1;
	if (!less(result, n))
		result[0] = 0;
	for (int bit = 31; bit >= 0; bit--) {
		result = multiplyMod(result, result, n);
		if (exponent >> bit & 1)
			result = multiplyMod(result, b, n);
	}
	return toBytes(result);
}

std::vector<uint8_t> encrypt(std::span<const uint8_t> modulus, std::span<const uint8_t> plain) {
	if (plain.size() % BLOCK_SIZE != 0)
		throw std::invalid_argument("RSA plaintext size is not a multiple of 128: " + std::to_string(plain.size()));
	std::vector<uint8_t> encrypted(plain.size());
	for (size_t offset = 0; offset < plain.size(); offset += BLOCK_SIZE) {
		const std::array<uint8_t, BLOCK_SIZE> block = modPow(plain.subspan(offset, BLOCK_SIZE), PUBLIC_EXPONENT, modulus);
		std::copy(block.begin(), block.end(), encrypted.begin() + static_cast<ptrdiff_t>(offset));
	}
	return encrypted;
}

} // namespace aion::clientprotocol::rsa
