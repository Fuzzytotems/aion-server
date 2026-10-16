#include "aion/gameserver/model/team/common/events/PlayerStopMentoringEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::common::events {

using gameobjects::player::Player;
using network::aion::serverpackets::SM_ABYSS_RANK_UPDATE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

PlayerStopMentoringEvent::PlayerStopMentoringEvent(TemporaryPlayerTeam& teamValue, Player& playerValue) : team(teamValue), player(playerValue) {
}

void PlayerStopMentoringEvent::handleEvent() {
	player->setMentor(false);
	PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_MENTOR_END());
	team->forEach([this](gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		if (!player->equals(member))
			PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_MSG_MENTOR_END_PARTYMSG(player->getName()));
		sendGroupPacketOnMentorEnd(member);
	});
	PacketSendUtility::broadcastPacketAndReceive(*player, SM_ABYSS_RANK_UPDATE(2, *player));
}

} // namespace aion::gameserver::model::team::common::events
