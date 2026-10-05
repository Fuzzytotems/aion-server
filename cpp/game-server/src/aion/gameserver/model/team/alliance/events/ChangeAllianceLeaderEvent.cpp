#include "aion/gameserver/model/team/alliance/events/ChangeAllianceLeaderEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/collections/Predicates.h"

namespace aion::gameserver::model::team::alliance::events {

using gameobjects::player::Player;
using network::aion::serverpackets::SM_ALLIANCE_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

ChangeAllianceLeaderEvent::ChangeAllianceLeaderEvent(PlayerAlliance& teamValue, Player& eventPlayerValue)
	: ChangeLeaderEvent(teamValue, runtime::Ptr<Player>(eventPlayerValue)) {
}

ChangeAllianceLeaderEvent::ChangeAllianceLeaderEvent(PlayerAlliance& teamValue) : ChangeLeaderEvent(teamValue, nullptr) {
}

PlayerAlliance& ChangeAllianceLeaderEvent::alliance() const {
	return *runtime::cast<PlayerAlliance>(team);
}

void ChangeAllianceLeaderEvent::handleEvent() {
	PlayerAlliance& allianceValue = alliance();
	if (!eventPlayer) {
		for (int32_t viceCaptainId : allianceValue.getViceCaptainIds().snapshot()) { // Java: the CopyOnWriteArrayList's snapshot iterator
			runtime::Ptr<PlayerAllianceMember> viceCaptain = allianceValue.getMember(viceCaptainId);
			if (!viceCaptain)
				throw runtime::NullPointerException("PlayerAlliance.getMember(" + std::to_string(viceCaptainId) + ")");
			if (viceCaptain->isOnline()) {
				changeLeaderTo(viceCaptain->getPlayer());
				return;
			}
		}
		changeLeaderToNextAvailablePlayer();
	} else {
		runtime::Ptr<Player> oldLeader = allianceValue.getLeaderObject();
		changeLeaderTo(*eventPlayer);
		PlayerAllianceService::changeViceCaptain(*oldLeader, AssignViceCaptainEvent_AssignType::DEMOTE_CAPTAIN_TO_VICECAPTAIN);
	}
}

void ChangeAllianceLeaderEvent::changeLeaderTo(Player& player) {
	PlayerAlliance& allianceValue = alliance();
	const bool inLeague = allianceValue.isInLeague();
	// Java: team.changeLeader(team.getMember(player.getObjectId())) - a null member is changeLeader's NullPointerException
	runtime::Ptr<PlayerAllianceMember> newLeader = allianceValue.getMember(player.getObjectId());
	if (!newLeader)
		throw runtime::NullPointerException("New leader should not be null");
	allianceValue.changeLeader(*newLeader);
	allianceValue.getViceCaptainIds().remove(player.getObjectId());
	if (inLeague) {
		// java-race: alliance → league here; league → alliance in every league event (m5g-plan.md §2.11 item 3)
		// lockdep: Java takes the league, and through it the other alliances, under this alliance's lock
		runtime::LockdepSuppression suppression("Java takes the league, and through it the other alliances, under this alliance's lock (m5g-plan.md D4)");
		allianceValue.getLeague()->broadcast();
	}
	const bool announce = static_cast<bool>(eventPlayer);
	allianceValue.forEach([&](gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		if (!inLeague) {
			PacketSendUtility::sendPacket(member, SM_ALLIANCE_INFO(allianceValue));
		}
		if (!player.equals(member)) {
			// eventPlayer null only when leader leave by own will and wee not must inform him about new leader
			if (announce) {
				PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_FORCE_HE_IS_NEW_LEADER(player.getName()));
			}
			if (inLeague) {
				// java-race: alliance → league here; league → alliance in every league event (m5g-plan.md §2.11 item 3)
				// lockdep: Java takes the league, and through it the other alliances, under this alliance's lock
				runtime::LockdepSuppression suppression("Java takes the league, and through it the other alliances, under this alliance's lock (m5g-plan.md D4)");
				runtime::Ptr<team::league::League> league = allianceValue.getLeague();
				runtime::Ptr<Player> captain = league->getCaptain();
				if (!captain)
					throw runtime::NullPointerException("League.getCaptain()");
				if (captain->equals(player)) {
					const auto allExcept = utils::collections::Predicates::Players::allExcept(player);
					league->forEach([&](gameobjects::AionObject& other) {
						SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_LEADER_TIMEOUT(player.getName());
						runtime::cast<PlayerAlliance>(other)->sendPacket(
							[&allExcept](gameobjects::AionObject& object2) { return allExcept(*runtime::cast<Player>(object2)); }, {message});
					});
				}
			}
		} else {
			PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_FORCE_YOU_BECOME_NEW_LEADER());
			if (inLeague) {
				runtime::Ptr<Player> captain = allianceValue.getLeague()->getCaptain();
				if (!captain)
					throw runtime::NullPointerException("League.getCaptain()");
				if (captain->equals(player))
					PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_UNION_YOU_BECOME_NEW_LEADER_TIMEOUT());
			}
		}
	});
}

} // namespace aion::gameserver::model::team::alliance::events
