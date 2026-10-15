#include "aion/gameserver/network/aion/clientpackets/CM_MAC_ADDRESS.h"

#include <memory>
#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

namespace {

/** Java regex class [a-zA-Z0-9] */
bool isAlphanumeric(char16_t c) noexcept {
	return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9');
}

/** Java regex class [a-zA-Z0-9_-] */
bool isAlphanumericUnderscoreDash(char16_t c) noexcept {
	return isAlphanumeric(c) || c == u'_' || c == u'-';
}

/** Java: hddSerial.matches("[0-9a-zA-Z _-]+") */
bool matchesSerialCharacters(std::u16string_view s) noexcept {
	if (s.empty())
		return false;
	for (char16_t c : s) {
		if (!isAlphanumericUnderscoreDash(c) && c != u' ')
			return false;
	}
	return true;
}

/**
 * Java: hddSerial.matches("^[a-zA-Z0-9] [a-zA-Z0-9_-].*|.*[a-zA-Z0-9_-] [a-zA-Z0-9]$"). Only reached with text of the class [0-9a-zA-Z _-] (see
 * fixHddSerial), which contains no line terminator, so `.*` matches any rest.
 */
bool hasSwappedCharacters(std::u16string_view s) noexcept {
	if (s.size() < 3)
		return false;
	const size_t n = s.size();
	return (isAlphanumeric(s[0]) && s[1] == u' ' && isAlphanumericUnderscoreDash(s[2])) ||
		(isAlphanumericUnderscoreDash(s[n - 3]) && s[n - 2] == u' ' && isAlphanumeric(s[n - 1]));
}

/** Java String.trim(): removes leading and trailing characters <= U+0020 */
std::u16string_view javaTrim(std::u16string_view s) noexcept {
	size_t begin = 0;
	size_t end = s.size();
	while (begin < end && s[begin] <= u' ')
		begin++;
	while (end > begin && s[end - 1] <= u' ')
		end--;
	return s.substr(begin, end - begin);
}

} // namespace

CM_MAC_ADDRESS::CM_MAC_ADDRESS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_MAC_ADDRESS::readImpl() {
	readC(); // unk
	int32_t routeSteps = readUH();
	for (int32_t i = 0; i < routeSteps; i++)
		readD(); // ip traceroute, see -> System.out.println(NetworkUtils.intToIpString(readD()))
	macAddress = readS();
	hddSerial = fixHddSerial(readS());
	readD(); // local IP, see -> System.out.println(NetworkUtils.intToIpString(readD()))
}

void CM_MAC_ADDRESS::runImpl() {
	const std::shared_ptr<AionConnection>& con = getConnection();
	con->setMacAddress(macAddress);
	con->setHddSerial(hddSerial);
	loginserver::LoginServer::getInstance().authenticateClient(con.get());
}

std::string CM_MAC_ADDRESS::fixHddSerial(std::string_view hddSerialValue) {
	using commons::utils::StringUtils::toUtf16;
	using commons::utils::StringUtils::toUtf8;
	const std::u16string serial = toUtf16(hddSerialValue);
	if (!serial.empty() && (serial.size() <= 2 || !matchesSerialCharacters(serial))) {
		// not a serial number string (some clients send weird but deterministic data)
		static constexpr const char* HEX_DIGITS = "0123456789ABCDEF";
		std::string hex = "0x";
		hex.reserve(2 + serial.size() * 4);
		for (char16_t unit : serial) { // UTF_16LE: low byte first
			const auto low = static_cast<uint8_t>(unit & 0xFF);
			const auto high = static_cast<uint8_t>(unit >> 8);
			hex += HEX_DIGITS[low >> 4];
			hex += HEX_DIGITS[low & 0xF];
			hex += HEX_DIGITS[high >> 4];
			hex += HEX_DIGITS[high & 0xF];
		}
		return hex;
	} else if (hasSwappedCharacters(serial)) { // check if second or penultimate char is a space
		// Java: hddSerial.replaceAll("(.)(.)", "$2$1").trim(); // for some reason some clients send switched chars
		std::u16string swapped = serial;
		for (size_t i = 0; i + 1 < swapped.size(); i += 2)
			std::swap(swapped[i], swapped[i + 1]);
		return toUtf8(javaTrim(swapped));
	}
	return toUtf8(javaTrim(serial));
}

AION_CLIENT_PACKET(CM_MAC_ADDRESS);

} // namespace aion::gameserver::network::aion::clientpackets
