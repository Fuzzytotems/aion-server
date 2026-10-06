#include "aion/gameserver/model/team/alliance/events/PlayerAllianceEnteredEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_MEMBER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::alliance::events {

using common::legacy::PlayerAllianceEvent;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ABYSS_RANK_UPDATE;
using network::aion::serverpackets::SM_ALLIANCE_INFO;
using network::aion::serverpackets::SM_ALLIANCE_MEMBER_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

PlayerAllianceEnteredEvent::PlayerAllianceEnteredEvent(PlayerAlliance& allianceValue, Player& playerValue)
	: PlayerEnteredEvent(allianceValue, playerValue) {
}

void PlayerAllianceEnteredEvent::handleEvent() {
	PlayerAlliance& allianceValue = *runtime::cast<PlayerAlliance>(team);
	Player& playerValue = *player;
	runtime::Ptr<PlayerAllianceMember> invitedMember = PlayerAllianceService::addPlayerToAlliance(allianceValue, playerValue);

	SM_ALLIANCE_INFO allianceInfo(allianceValue);
	SM_ALLIANCE_MEMBER_INFO allianceMemberInfo(*invitedMember, PlayerAllianceEvent::JOIN);
	PacketSendUtility::sendPacket(playerValue, allianceInfo);
	PacketSendUtility::sendPacket(playerValue, SM_SYSTEM_MESSAGE::STR_FORCE_ENTERED_FORCE());
	PacketSendUtility::sendPacket(playerValue, allianceMemberInfo);
	allianceValue.sendBrands(playerValue);
	allianceValue.forEachTeamMember([&](TeamMember& member) {
		Player& p = *runtime::cast<Player>(member.getObject());
		if (!playerValue.equals(p)) {
			PacketSendUtility::sendPacket(p, allianceMemberInfo);
			PacketSendUtility::sendPacket(p, SM_SYSTEM_MESSAGE::STR_FORCE_HE_ENTERED_FORCE(playerValue.getName()));
			PacketSendUtility::sendPacket(p, allianceInfo);
			PacketSendUtility::sendPacket(playerValue, SM_ALLIANCE_MEMBER_INFO(*runtime::cast<PlayerAllianceMember>(member), PlayerAllianceEvent::ENTER));
		}
	});
	PacketSendUtility::broadcastPacket(playerValue, SM_ABYSS_RANK_UPDATE(1, playerValue), true);

	if (allianceValue.isInLeague()) {
		// java-race: alliance → league here; league → alliance in every league event (m5g-plan.md §2.11 item 3)
		// lockdep: Java takes the league, and through it the other alliances, under this alliance's lock
		runtime::LockdepSuppression suppression("Java takes the league, and through it the other alliances, under this alliance's lock (m5g-plan.md D4)");
		allianceValue.getLeague()->broadcast();
	}
	PlayerEnteredEvent::handleEvent();
}

} // namespace aion::gameserver::model::team::alliance::events
