#include "aion/loginserver/network/ncrypt/CryptEngine.h"

#include <array>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"

namespace aion::loginserver::network::ncrypt {

namespace {

/** static key for the first packet sent to the client */
constexpr std::array<uint8_t, 16> INITIAL_KEY = {0x6b, 0x60, 0xcb, 0x5b, 0x82, 0xce, 0x90, 0xb1, 0xcc, 0x2b, 0x6c, 0x55, 0x6c, 0x6c, 0x6c, 0x6c};

uint32_t readWord(std::span<const uint8_t> data, size_t pos) noexcept {
	return static_cast<uint32_t>(data[pos]) | static_cast<uint32_t>(data[pos + 1]) << 8 | static_cast<uint32_t>(data[pos + 2]) << 16 |
		static_cast<uint32_t>(data[pos + 3]) << 24;
}

void writeWord(std::span<uint8_t> data, size_t pos, uint32_t value) noexcept {
	data[pos] = static_cast<uint8_t>(value);
	data[pos + 1] = static_cast<uint8_t>(value >> 8);
	data[pos + 2] = static_cast<uint8_t>(value >> 16);
	data[pos + 3] = static_cast<uint8_t>(value >> 24);
}

} // namespace

CryptEngine::CryptEngine() : key(INITIAL_KEY.begin(), INITIAL_KEY.end()), cipher(key) {
}

void CryptEngine::updateKey(std::span<const uint8_t> newKey) {
	std::vector<uint8_t> copy(newKey.begin(), newKey.end());
	std::scoped_lock lock(mutex);
	key = std::move(copy);
}

bool CryptEngine::decrypt(std::span<uint8_t> data) {
	std::scoped_lock lock(mutex);
	cipher.decipher(data);
	return verifyChecksum(data);
}

int32_t CryptEngine::encrypt(std::span<uint8_t> data, int32_t length) {
	using namespace commons::utils;
	std::scoped_lock lock(mutex);
	// Java's int arithmetic in 64 bits (% truncates toward zero in both languages; a payload of only an opcode gives length -1)
	int64_t newLength = static_cast<int64_t>(length) + 4;
	if (!updatedKey) // the key is not updated, so the first packet is encrypted with the initial key
		newLength += 4;
	newLength += 8 - newLength % 8;
	// Deviation: for lengths giving no block at all Java writes the checksum/XOR key outside the encrypted range
	if (newLength < 8)
		throw IllegalArgumentException("Invalid packet length: " + std::to_string(length));
	if (static_cast<uint64_t>(newLength) > data.size())
		throw IndexOutOfBoundsException("Encrypted packet size " + std::to_string(newLength) + " exceeds the buffer size " + std::to_string(data.size()));

	auto packet = data.first(static_cast<size_t>(newLength));
	if (!updatedKey) {
		// Deviation: Java throws ArrayIndexOutOfBoundsException from BlowfishCipher.updateKey after the packet was encrypted, leaving a broken
		// cipher behind. Here nothing is modified.
		if (key.empty())
			throw IllegalStateException("Blowfish session key is empty");
		encXORPass(packet, static_cast<uint32_t>(Rnd::nextInt()));
		cipher.cipher(packet);
		cipher.updateKey(key);
		updatedKey = true;
	} else {
		appendChecksum(packet);
		cipher.cipher(packet);
	}
	return static_cast<int32_t>(newLength);
}

bool CryptEngine::verifyChecksum(std::span<const uint8_t> data) noexcept {
	if ((data.size() & 3) != 0 || data.size() <= 4)
		return false;

	// Java also reads the last word into a variable that is never used, so it is not part of the checksum
	uint32_t chksum = 0;
	for (size_t i = 0; i < data.size() - 4; i += 4)
		chksum ^= readWord(data, i);
	return chksum == 0;
}

void CryptEngine::appendChecksum(std::span<uint8_t> raw) noexcept {
	uint32_t chksum = 0;
	size_t i = 0;
	for (; i < raw.size() - 4; i += 4)
		chksum ^= readWord(raw, i);
	writeWord(raw, i, chksum);
}

void CryptEngine::encXORPass(std::span<uint8_t> data, uint32_t key) noexcept {
	size_t stop = data.size() - 8; // Java: int stop = length - 8 (>= 0 here)
	size_t pos = 4;
	uint32_t ecx = key;

	while (pos < stop) {
		uint32_t edx = readWord(data, pos);
		ecx += edx;
		edx ^= ecx;
		writeWord(data, pos, edx);
		pos += 4;
	}
	writeWord(data, pos, ecx);
}

} // namespace aion::loginserver::network::ncrypt
