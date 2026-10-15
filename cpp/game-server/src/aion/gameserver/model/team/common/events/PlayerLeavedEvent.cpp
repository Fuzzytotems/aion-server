#include "aion/gameserver/model/team/common/events/PlayerLeavedEvent.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEAVE_GROUP_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::model::team::common::events {

using gameobjects::player::Player;
using network::aion::serverpackets::SM_LEAVE_GROUP_MEMBER;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

namespace {

/** Java: team.equals(leavedPlayer.getPosition().getWorldMapInstance().getRegisteredTeam()) - AionObject.equals of a possibly null team */
bool isRegisteredTeamOf(TemporaryPlayerTeam& team, Player& player) {
	runtime::Ptr<GeneralTeam> registeredTeam = player.getPosition()->getWorldMapInstance()->getRegisteredTeam();
	return registeredTeam && team.equals(*registeredTeam);
}

} // namespace

PlayerLeavedEvent::PlayerLeavedEvent(TemporaryPlayerTeam& teamValue, Player& player) : PlayerLeavedEvent(teamValue, player, LeaveReson::LEAVE) {
}

PlayerLeavedEvent::PlayerLeavedEvent(TemporaryPlayerTeam& teamValue, Player& player, LeaveReson reasonValue)
	: PlayerLeavedEvent(teamValue, player, reasonValue, "") {
}

PlayerLeavedEvent::PlayerLeavedEvent(TemporaryPlayerTeam& teamValue, Player& player, LeaveReson reasonValue, std::string_view banPersonNameValue)
	: team(teamValue), leavedPlayer(player), reason(reasonValue), banPersonName(banPersonNameValue) {
}

bool PlayerLeavedEvent::checkCondition() {
	return team->hasMember(leavedPlayer->getObjectId());
}

void PlayerLeavedEvent::handleEvent() {
	if (leavedPlayer->isOnline()) {
		PacketSendUtility::sendPacket(*leavedPlayer, SM_LEAVE_GROUP_MEMBER());
		if (isRegisteredTeamOf(*team, *leavedPlayer)) {
			PacketSendUtility::sendPacket(*leavedPlayer, SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE_NOT_PARTY());
			// lambda at PlayerLeavedEvent.java:61 (fieldmap.toml [kinds] K5): the task captures the leaver and the team as Refs instead of the event
			leavedPlayer->getController().addTask(TaskId::INSTANCE_KICK,
				utils::ThreadPoolManager::getInstance().schedule(runtime::bindTask(
																	 [](Player& kickedPlayer, TemporaryPlayerTeam& leftTeam) {
																		 if (kickedPlayer.getCurrentTeamId() != leftTeam.getObjectId()) {
																			 if (isRegisteredTeamOf(leftTeam, kickedPlayer))
																				 services::instance::InstanceService::moveToExitPoint(kickedPlayer);
																		 }
																	 },
																	 runtime::Ref<Player>(leavedPlayer), runtime::Ref<TemporaryPlayerTeam>(team)),
					30000));
		}
	}
	services::event::EventService::getInstance().onLeftTeam(*leavedPlayer, *team);
}

} // namespace aion::gameserver::model::team::common::events
