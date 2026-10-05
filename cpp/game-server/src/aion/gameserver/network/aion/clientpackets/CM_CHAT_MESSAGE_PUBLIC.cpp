#include "aion/gameserver/network/aion/clientpackets/CM_CHAT_MESSAGE_PUBLIC.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/ChatTypeInfo.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/services/NameRestrictionService.h"
#include "aion/gameserver/services/player/PlayerChatService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::ChatType;
using model::gameobjects::player::Player;
using serverpackets::SM_MESSAGE;
using utils::PacketSendUtility;

CM_CHAT_MESSAGE_PUBLIC::CM_CHAT_MESSAGE_PUBLIC(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CHAT_MESSAGE_PUBLIC.java:37-40
void CM_CHAT_MESSAGE_PUBLIC::readImpl() {
	type = model::getChatType(readC()); // an unknown id is Java's IllegalArgumentException
	message = readS();
}

// Java CM_CHAT_MESSAGE_PUBLIC.java:43-97
void CM_CHAT_MESSAGE_PUBLIC::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();

	if (utils::chathandlers::ChatProcessor::getInstance().handleChatCommand(*player, message))
		return;

	if (!restrictions::PlayerRestrictions::canChat(player))
		return;

	services::player::PlayerChatService::logMessage(*player, type, message);
	message = services::NameRestrictionService::filterMessage(message);

	switch (type) {
		case ChatType::GROUP:
			if (!player->isInTeam())
				return;
			broadcastToGroupMembers(*player);
			break;
		case ChatType::ALLIANCE:
			if (!player->isInAlliance())
				return;
			broadcastToAllianceMembers(*player);
			break;
		case ChatType::GROUP_LEADER:
			if (!player->isInTeam())
				return;
			// Alert must go to entire group or alliance.
			if (player->isInGroup())
				broadcastToGroupMembers(*player);
			else
				broadcastToAllianceMembers(*player);
			break;
		case ChatType::LEGION:
			if (!player->isLegionMember())
				return;
			broadcastToLegionMembers(*player);
			break;
		case ChatType::LEAGUE:
		case ChatType::LEAGUE_ALERT:
			if (!player->isInLeague())
				return;
			broadcastToLeagueMembers(*player);
			break;
		case ChatType::NORMAL:
		case ChatType::SHOUT:
			broadcastToPlayers(*player);
			break;
		case ChatType::COMMAND:
			if (player->getAbyssRank()->getRank() == utils::stats::AbyssRankEnum::COMMANDER ||
				player->getAbyssRank()->getRank() == utils::stats::AbyssRankEnum::SUPREME_COMMANDER)
				broadcastFromCommander(*player);
			break;
		default:
			if (!player->isStaff())
				return;
			broadcastToPlayers(*player);
			break;
	}
}

// Java CM_CHAT_MESSAGE_PUBLIC.java:99-103
void CM_CHAT_MESSAGE_PUBLIC::broadcastFromCommander(Player& player) {
	const int32_t senderRace = model::getRaceId(player.getRace());
	PacketSendUtility::broadcastPacket(player, SM_MESSAGE(player, message, type), true,
		[senderRace, &player](Player& p) { return senderRace == model::getRaceId(p.getRace()) || player.isStaff() || p.isStaff(); });
}

// Java CM_CHAT_MESSAGE_PUBLIC.java:110-113
void CM_CHAT_MESSAGE_PUBLIC::broadcastToPlayers(Player& player) {
	PacketSendUtility::broadcastPacket(player, SM_MESSAGE(player, message, type), true,
		[&player](Player& p) { return !p.getBlockList()->contains(player.getObjectId()) || player.isStaff() || p.isStaff(); });
}

// Java CM_CHAT_MESSAGE_PUBLIC.java:120-122
void CM_CHAT_MESSAGE_PUBLIC::broadcastToGroupMembers(Player& player) {
	SM_MESSAGE packet(player, message, type);
	player.getCurrentGroup()->sendPackets({packet});
}

// Java CM_CHAT_MESSAGE_PUBLIC.java:129-131
void CM_CHAT_MESSAGE_PUBLIC::broadcastToAllianceMembers(Player& player) {
	SM_MESSAGE packet(player, message, type);
	player.getPlayerAlliance()->sendPackets({packet});
}

// Java CM_CHAT_MESSAGE_PUBLIC.java:138-140
void CM_CHAT_MESSAGE_PUBLIC::broadcastToLeagueMembers(Player& player) {
	SM_MESSAGE packet(player, message, type);
	player.getPlayerAlliance()->getLeague()->sendPackets({packet});
}

// Java CM_CHAT_MESSAGE_PUBLIC.java:147-149
void CM_CHAT_MESSAGE_PUBLIC::broadcastToLegionMembers(Player& player) {
	PacketSendUtility::broadcastToLegion(*player.getLegion(), SM_MESSAGE(player, message, type));
}

AION_CLIENT_PACKET(CM_CHAT_MESSAGE_PUBLIC);

} // namespace aion::gameserver::network::aion::clientpackets
