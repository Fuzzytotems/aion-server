#include "aion/gameserver/model/team/alliance/events/PlayerDisconnectedEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/alliance/events/ChangeAllianceLeaderEvent.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_MEMBER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::alliance::events {

using common::legacy::PlayerAllianceEvent;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ALLIANCE_INFO;
using network::aion::serverpackets::SM_ALLIANCE_MEMBER_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

PlayerDisconnectedEvent::PlayerDisconnectedEvent(PlayerAlliance& allianceValue, Player& player)
	: alliance(allianceValue), disconnected(player), disconnectedMember(allianceValue.getMember(player.getObjectId())) {
}

bool PlayerDisconnectedEvent::checkCondition() {
	return alliance->hasMember(disconnected->getObjectId());
}

void PlayerDisconnectedEvent::handleEvent() {
	if (!disconnectedMember)
		throw runtime::NullPointerException("Disconnected member should not be null");
	PlayerAlliance& allianceValue = *alliance;
	Player& disconnectedValue = *disconnected;
	PlayerAllianceMember& member = *disconnectedMember;
	runtime::Ptr<Player> leader = allianceValue.getLeaderObject();

	if (disconnectedValue.equals(*leader)) {
		ChangeAllianceLeaderEvent event(allianceValue);
		allianceValue.onEvent(event);
	}

	allianceValue.forEach([&](gameobjects::AionObject& object) {
		Player& player = *runtime::cast<Player>(object);
		if (!disconnectedValue.equals(player)) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_FORCE_HE_BECOME_OFFLINE(disconnectedValue.getName()));
			PacketSendUtility::sendPacket(player, SM_ALLIANCE_MEMBER_INFO(member, PlayerAllianceEvent::DISCONNECTED));
			PacketSendUtility::sendPacket(player, SM_ALLIANCE_INFO(allianceValue));
		}
	});

	if (allianceValue.getOnlineMembers().empty()) {
		PlayerAllianceService::disband(allianceValue, false);
	} else if (allianceValue.isInLeague()) {
		// java-race: alliance → league here; league → alliance in every league event (m5g-plan.md §2.11 item 3)
		// lockdep: Java takes the league, and through it the other alliances, under this alliance's lock
		runtime::LockdepSuppression suppression("Java takes the league, and through it the other alliances, under this alliance's lock (m5g-plan.md D4)");
		allianceValue.getLeague()->broadcast(disconnectedValue);
	}
}

} // namespace aion::gameserver::model::team::alliance::events
