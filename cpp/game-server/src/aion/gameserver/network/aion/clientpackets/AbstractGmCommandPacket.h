#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The GM command packets (CM_BUILDER_COMMAND, CM_BUILDER_CONTROL, CM_DEBUG_COMMAND): one string, handed to ChatProcessor.handleConsoleCommand.
 * <p>
 * C++: replaceUnsupportedCommandChars works on UTF-16 text (Java's regex character class `[^\u0000-ľ]` matches code points of the UTF-16
 * string: a surrogate pair is one match and one "?", a lone surrogate one match as well); ChatUtil.getRealCharName, its only caller, works in
 * UTF-16 too.
 */
class AbstractGmCommandPacket : public AionClientPacket {
public:
	/** client sends this for each unsupported char in the command */
	static constexpr std::u16string_view UNSUPPORTED_COMMAND_CHAR_PLACEHOLDER = u"?";

protected:
	std::string command;

	AbstractGmCommandPacket(int32_t opcode, const StateSet& validStates);

	void readImpl() override;
	void runImpl() override;

public:
	~AbstractGmCommandPacket() override;

	static std::u16string replaceUnsupportedCommandChars(std::u16string_view input);
};

} // namespace aion::gameserver::network::aion::clientpackets
