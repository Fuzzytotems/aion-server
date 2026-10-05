#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Packet that reads Whisper chat messages (C_WHISPER).<br>
 *
 * @author SoulKeeper
 */
class CM_CHAT_MESSAGE_WHISPER : public AionClientPacket {
	friend struct ChatPacketsTestAccess;

private:
	/** To whom this message is sent */
	std::string name;
	/** Message text */
	std::string message;

public:
	CM_CHAT_MESSAGE_WHISPER(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
