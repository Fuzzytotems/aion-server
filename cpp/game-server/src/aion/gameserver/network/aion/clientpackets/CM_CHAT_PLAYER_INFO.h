#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client asks for a chat window with a player by name (C_ASK_PC_INFO).
 *
 * @author prix, Neon
 */
class CM_CHAT_PLAYER_INFO : public AionClientPacket {
private:
	std::string playerName;

public:
	CM_CHAT_PLAYER_INFO(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
