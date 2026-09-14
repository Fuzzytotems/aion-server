#pragma once

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aion::gameserver::network::aion::serverpackets {

/**
 * TEST STUB, not the port of SM_SYSTEM_MESSAGE: the minimal class that the generated member block SM_SYSTEM_MESSAGE.gen.h and the definitions
 * SM_SYSTEM_MESSAGE.gen0..7.cpp need (the contract documented in cpp/tools/gen/sysmsg.py). It lets aion_gs_network_crypt_tests compile the
 * generated code before the real class (P4-06, which derives from AionServerPacket) exists.
 * <p>
 * This header sits in the test directory at the include path of the real header, so the test executable picks it up (the test directory is
 * searched before src/). Do not include it from anything but the generated-code tests of tests/network_crypt.
 */
class SM_SYSTEM_MESSAGE {
public:
	SM_SYSTEM_MESSAGE(int32_t msgId, std::vector<std::string> params) : msgId(msgId), params(std::move(params)) {}

	int32_t getId() const noexcept { return msgId; }
	const std::vector<std::string>& getParams() const noexcept { return params; }

	static std::string toJavaString(int32_t value) { return std::to_string(value); }
	static std::string toJavaString(int64_t value) { return std::to_string(value); }
	static std::string toJavaString(int8_t value) { return std::to_string(static_cast<int32_t>(value)); }

	/** Stub: shortest round-trip digits in fixed notation plus ".0" if needed; Java's Float.toString only for 1e-3 <= |value| < 1e7 */
	static std::string toJavaString(float value) {
		char buffer[64];
		const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed);
		std::string text(buffer, result.ptr);
		if (text.find('.') == std::string::npos)
			text += ".0";
		return text;
	}

#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.gen.h"

private:
	int32_t msgId;
	std::vector<std::string> params;
};

} // namespace aion::gameserver::network::aion::serverpackets
