#include "aion/gameserver/model/team/group/events/PlayerStartMentoringEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_MEMBER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/collections/Predicates.h"

namespace aion::gameserver::model::team::group::events {

using gameobjects::player::Player;
using network::aion::serverpackets::SM_ABYSS_RANK_UPDATE;
using network::aion::serverpackets::SM_GROUP_MEMBER_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

PlayerStartMentoringEvent::PlayerStartMentoringEvent(PlayerGroup& groupValue, Player& playerValue) : group(groupValue), player(playerValue) {
}

void PlayerStartMentoringEvent::handleEvent() {
	const auto canBeMentored = utils::collections::Predicates::Players::canBeMentoredBy(*player);
	if (group->filterMembers([&canBeMentored](gameobjects::AionObject& object) { return canBeMentored(*runtime::cast<Player>(object)); }).empty()) {
		utils::audit::AuditLogger::log(*player, "sent fake start mentoring packet");
		return;
	}
	player->setMentor(true);
	PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_MENTOR_START());
	group->forEach([this](gameobjects::AionObject& object) { accept(*runtime::cast<Player>(object)); });
	PacketSendUtility::broadcastPacketAndReceive(*player, SM_ABYSS_RANK_UPDATE(2, *player));
}

void PlayerStartMentoringEvent::accept(Player& member) {
	if (!player->equals(member)) {
		PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_MSG_MENTOR_START_PARTYMSG(player->getName()));
	}
	PacketSendUtility::sendPacket(member, SM_GROUP_MEMBER_INFO(*group, *player, common::legacy::GroupEvent::MOVEMENT));
}

} // namespace aion::gameserver::model::team::group::events
