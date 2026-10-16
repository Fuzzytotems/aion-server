#include "aion/gameserver/network/EncryptionKeyPair.h"

#include <format>
#include <iterator>

#include "aion/commons/utils/TimeUtils.h"

namespace aion::gameserver::network {

EncryptionKeyPair::EncryptionKeyPair(int32_t baseKey) : baseKey(baseKey) {
	const auto key = static_cast<uint32_t>(baseKey);
	keys[SERVER] = {static_cast<uint8_t>(key & 0xff), static_cast<uint8_t>((key >> 8) & 0xff), static_cast<uint8_t>((key >> 16) & 0xff),
		static_cast<uint8_t>((key >> 24) & 0xff), 0xa1, 0x6c, 0x54, 0x87};
	keys[CLIENT] = keys[SERVER];
	lastUpdate = commons::utils::currentTimeMillis();
}

std::string EncryptionKeyPair::toString() const {
	std::string sb = "{client:0x";
	for (uint8_t b : keys[CLIENT])
		std::format_to(std::back_inserter(sb), "{:x}", b);
	sb += ",server:0x";
	for (uint8_t b : keys[SERVER])
		std::format_to(std::back_inserter(sb), "{:x}", b);
	std::format_to(std::back_inserter(sb), ",base:0x{:x},update:{}}}", static_cast<uint32_t>(baseKey), lastUpdate);
	return sb;
}

bool EncryptionKeyPair::validateClientPacket(std::span<const uint8_t> data) noexcept {
	// Java: buf.limit() >= 5 && buf.getShort(0) == ~buf.getShort(3) && buf.get(2) == staticClientPacketCode (little endian shorts; comparing the
	// sign-extended values is the same as comparing the 16 bits)
	if (data.size() < 5)
		return false;
	const auto first = static_cast<uint16_t>(data[0] | (data[1] << 8));
	const auto second = static_cast<uint16_t>(data[3] | (data[4] << 8));
	return first == static_cast<uint16_t>(~second) && data[2] == staticClientPacketCode;
}

void EncryptionKeyPair::addToKey(std::array<uint8_t, 8>& key, uint64_t size) noexcept {
	uint64_t value = 0;
	for (int i = 7; i >= 0; i--)
		value = (value << 8) | key[i];
	value += size;
	for (size_t i = 0; i < 8; i++)
		key[i] = static_cast<uint8_t>(value >> (8 * i));
}

bool EncryptionKeyPair::decrypt(std::span<uint8_t> data) noexcept {
	const size_t size = data.size();
	if (size == 0)
		return false;
	const std::array<uint8_t, 8>& clientPacketKey = keys[CLIENT];

	// prev encrypted byte
	uint8_t prev = data[0];

	// decrypt first byte
	data[0] ^= clientPacketKey[0];

	// decrypt loop
	for (size_t i = 1; i < size; i++) {
		const uint8_t curr = data[i];
		data[i] ^= staticKey[i & 63] ^ clientPacketKey[i & 7] ^ prev;
		prev = curr;
	}

	if (validateClientPacket(data)) {
		// set key new value (Java: oldKey += size, only stored for a valid packet)
		addToKey(keys[CLIENT], size);
		return true;
	}
	return false;
}

void EncryptionKeyPair::encrypt(std::span<uint8_t> data) noexcept {
	const size_t size = data.size();
	std::array<uint8_t, 8>& serverPacketKey = keys[SERVER];
	if (size > 0) {
		// encrypt first byte
		data[0] ^= serverPacketKey[0];

		// prev encrypted byte
		uint8_t prev = data[0];

		// encrypt loop
		for (size_t i = 1; i < size; i++) {
			data[i] ^= staticKey[i & 63] ^ serverPacketKey[i & 7] ^ prev;
			prev = data[i];
		}
	}

	// change key
	addToKey(serverPacketKey, size);
}

} // namespace aion::gameserver::network
