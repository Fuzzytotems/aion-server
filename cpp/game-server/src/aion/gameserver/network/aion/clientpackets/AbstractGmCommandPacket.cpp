#include "aion/gameserver/network/aion/clientpackets/AbstractGmCommandPacket.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

AbstractGmCommandPacket::AbstractGmCommandPacket(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

AbstractGmCommandPacket::~AbstractGmCommandPacket() = default;

// Java AbstractGmCommandPacket.java:21-24
void AbstractGmCommandPacket::readImpl() {
	command = readS();
}

// Java AbstractGmCommandPacket.java:26-29. The packets are IN_GAME only: getActivePlayer() is set (a null one is Java's NPE)
void AbstractGmCommandPacket::runImpl() {
	utils::chathandlers::ChatProcessor::getInstance().handleConsoleCommand(*getConnection()->getActivePlayer(), command);
}

// Java AbstractGmCommandPacket.java:31-33: unsupportedCommandChars.matcher(input).replaceAll("?") with the pattern [^\u0000-ľ]
std::u16string AbstractGmCommandPacket::replaceUnsupportedCommandChars(std::u16string_view input) {
	std::u16string result;
	result.reserve(input.size());
	for (size_t i = 0; i < input.size(); ++i) {
		char16_t c = input[i];
		if (c <= 0x013E) {
			result += c;
			continue;
		}
		result += UNSUPPORTED_COMMAND_CHAR_PLACEHOLDER;
		if (c >= 0xD800 && c <= 0xDBFF && i + 1 < input.size() && input[i + 1] >= 0xDC00 && input[i + 1] <= 0xDFFF)
			++i; // the low surrogate belongs to the same code point
	}
	return result;
}

} // namespace aion::gameserver::network::aion::clientpackets
