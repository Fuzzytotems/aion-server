#include "aion/gameserver/model/team/group/events/PlayerGroupStopMentoringEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_MEMBER_INFO.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::group::events {

PlayerGroupStopMentoringEvent::PlayerGroupStopMentoringEvent(PlayerGroup& groupValue, gameobjects::player::Player& playerValue)
	: PlayerStopMentoringEvent(groupValue, playerValue) {
}

void PlayerGroupStopMentoringEvent::sendGroupPacketOnMentorEnd(gameobjects::player::Player& member) {
	utils::PacketSendUtility::sendPacket(member, network::aion::serverpackets::SM_GROUP_MEMBER_INFO(*runtime::cast<PlayerGroup>(team), *player,
													 common::legacy::GroupEvent::MOVEMENT));
}

} // namespace aion::gameserver::model::team::group::events
