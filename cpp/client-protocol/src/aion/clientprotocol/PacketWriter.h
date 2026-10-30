#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "aion/clientprotocol/Utf.h"

namespace aion::clientprotocol {

/** Little endian packet body builder with the Java write names (writeC/H/D/Q/F/S/B), for the client packets of ClientPackets. */
class PacketWriter {
public:
	PacketWriter& C(int32_t value) {
		data.push_back(static_cast<uint8_t>(value));
		return *this;
	}
	PacketWriter& H(int32_t value) {
		C(value);
		return C(value >> 8);
	}
	PacketWriter& D(int32_t value) {
		H(value);
		return H(value >> 16);
	}
	PacketWriter& Q(int64_t value) {
		D(static_cast<int32_t>(value));
		return D(static_cast<int32_t>(value >> 32));
	}
	PacketWriter& F(float value) { return D(static_cast<int32_t>(std::bit_cast<uint32_t>(value))); }
	/** UTF-16LE with terminating 0 char */
	PacketWriter& S(std::string_view text) {
		for (char16_t c : utf8ToUtf16(text))
			H(c);
		return H(0);
	}
	PacketWriter& B(std::span<const uint8_t> bytes) {
		data.insert(data.end(), bytes.begin(), bytes.end());
		return *this;
	}
	PacketWriter& zeros(size_t count) {
		data.insert(data.end(), count, uint8_t{0});
		return *this;
	}

	std::vector<uint8_t> data;
};

} // namespace aion::clientprotocol
