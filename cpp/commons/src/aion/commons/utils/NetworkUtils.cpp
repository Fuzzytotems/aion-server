#include "aion/commons/utils/NetworkUtils.h"

#include <algorithm>
#include <charconv>

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>
#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::utils::NetworkUtils {

std::optional<std::string> findLocalIPv4() {
	try {
		asio::io_context context;
		asio::ip::udp::socket socket(context);
		// connecting a UDP socket sends no packets, it only selects the outbound interface
		socket.connect(asio::ip::udp::endpoint(asio::ip::make_address_v4("1.1.1.1"), 80));
		return socket.local_endpoint().address().to_string();
	} catch (const std::exception& e) {
		logging::LoggerFactory::getLogger("com.aionemu.commons.utils.NetworkUtils").error("Could not find local IPv4 address", e);
		return std::nullopt;
	}
}

namespace {

std::optional<int> parseOctet(std::string_view s) {
	int value = 0;
	auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
	if (ec != std::errc() || ptr != s.data() + s.size() || value < 0 || value > 255)
		return std::nullopt;
	return value;
}

} // namespace

bool checkIPMatching(std::string_view pattern, std::string_view address) {
	if (pattern == "*.*.*.*" || pattern == "*")
		return true;

	std::vector<std::string> mask = StringUtils::splitJava(pattern, ".");
	std::vector<std::string> ipAddress = StringUtils::splitJava(address, ".");
	for (size_t i = 0; i < mask.size(); i++) {
		if (i >= ipAddress.size())
			return false; // Java would throw ArrayIndexOutOfBoundsException
		if (mask[i] == "*" || mask[i] == ipAddress[i])
			continue;
		if (mask[i].find('-') != std::string::npos) {
			std::vector<std::string> bounds = StringUtils::splitJava(mask[i], "-");
			if (bounds.size() < 2)
				return false;
			auto min = parseOctet(bounds[0]);
			auto max = parseOctet(bounds[1]);
			auto ip = parseOctet(ipAddress[i]);
			if (!min || !max || !ip || *ip < *min || *ip > *max)
				return false;
		} else {
			return false;
		}
	}
	return true;
}

std::string intToIpString(int32_t ip) {
	auto u = static_cast<uint32_t>(ip);
	return fmt::format("{}.{}.{}.{}", u & 0xFF, (u >> 8) & 0xFF, (u >> 16) & 0xFF, (u >> 24) & 0xFF);
}

std::string toHex(const ByteBuffer& buffer) {
	return toHex(buffer, 0, std::min(buffer.limit(), buffer.capacity()));
}

namespace {

void appendText(const ByteBuffer& buffer, std::string& result, int32_t startIndex, int32_t endIndex) {
	for (int32_t charPos = startIndex; charPos < endIndex; charPos++) {
		int c = static_cast<uint8_t>(buffer.data()[charPos]);
		result += (c > 0x1f && c < 0x80) ? static_cast<char>(c) : '.';
	}
}

} // namespace

std::string toHex(const ByteBuffer& buffer, int32_t start, int32_t end) {
	std::string result;
	if (start < end && (start < 0 || end > buffer.limit())) // Java: buffer.get(i) throws for the first index outside [0, limit)
		throw IndexOutOfBoundsException(fmt::format("Index {} out of bounds for length {}", start < 0 ? start : buffer.limit(), buffer.limit()));
	for (int32_t i = start, bytes = 0; i < end; bytes++) {
		if (bytes % 16 == 0) {
			if (!result.empty())
				result += '\n';
			result += fmt::format("{:04X}: ", bytes);
		}

		result += fmt::format("{:02X} ", static_cast<uint8_t>(buffer.data()[i]));

		int32_t bytesInRow = (bytes % 16) + 1;
		// Deviation: Java only prints the text column of the last row if end == capacity; here it is always printed
		if (++i == end || bytesInRow == 16) {
			for (int32_t j = bytesInRow; j <= 16; j++)
				result += "   ";
			appendText(buffer, result, i - bytesInRow, i);
		}
	}
	return result;
}

} // namespace aion::commons::utils::NetworkUtils
