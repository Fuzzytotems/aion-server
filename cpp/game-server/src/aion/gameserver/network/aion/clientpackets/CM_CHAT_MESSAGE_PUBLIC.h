#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Packet that reads normal chat messages (C_SAY).<br>
 *
 * @author SoulKeeper
 */
class CM_CHAT_MESSAGE_PUBLIC : public AionClientPacket {
	friend struct ChatPacketsTestAccess;

private:
	/** Chat type */
	model::ChatType type{};
	/** Chat message */
	std::string message;

public:
	CM_CHAT_MESSAGE_PUBLIC(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;

private:
	void broadcastFromCommander(model::gameobjects::player::Player& player);
	/** Sends message to all players that are not in blocklist (except GMs) */
	void broadcastToPlayers(model::gameobjects::player::Player& player);
	/** Sends message to all group members. */
	void broadcastToGroupMembers(model::gameobjects::player::Player& player);
	/** Sends message to all alliance members */
	void broadcastToAllianceMembers(model::gameobjects::player::Player& player);
	/** Sends message to all league members */
	void broadcastToLeagueMembers(model::gameobjects::player::Player& player);
	/** Sends message to all legion members */
	void broadcastToLegionMembers(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::network::aion::clientpackets
