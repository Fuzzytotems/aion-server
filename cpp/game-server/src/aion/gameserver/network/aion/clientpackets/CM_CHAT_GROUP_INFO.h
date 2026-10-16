#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Asks for the chat window info of a player (C_ASK_PARTY_INFO).
 *
 * @author ginho1, Neon
 */
class CM_CHAT_GROUP_INFO : public AionClientPacket {
private:
	std::string playerName;
	int32_t unk = 0; // Java @SuppressWarnings("unused")

public:
	CM_CHAT_GROUP_INFO(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
