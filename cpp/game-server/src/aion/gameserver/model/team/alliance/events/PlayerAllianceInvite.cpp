#include "aion/gameserver/model/team/alliance/events/PlayerAllianceInvite.h"

#include <algorithm>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/collections/Predicates.h"

namespace aion::gameserver::model::team::alliance::events {

using gameobjects::player::Player;
using group::PlayerGroup;
using group::PlayerGroupService;

PlayerAllianceInvite::PlayerAllianceInvite(Player& inviter) : RequestResponseHandler(runtime::Ptr<gameobjects::Creature>(inviter)) {
}

PlayerAllianceInvite::~PlayerAllianceInvite() = default;

runtime::Ref<PlayerAllianceInvite> PlayerAllianceInvite::create(Player& inviter) {
	return runtime::makeRef<PlayerAllianceInvite>(inviter);
}

void PlayerAllianceInvite::acceptRequest(runtime::Ptr<gameobjects::Creature> requesterValue, Player& invited) {
	Player& inviter = *runtime::cast<Player>(requesterValue);
	if (restrictions::PlayerRestrictions::canInviteToAlliance(inviter, invited)) {

		runtime::Ptr<PlayerAlliance> alliance = inviter.getPlayerAlliance();
		// Refs: a player removed from his group below is no team's member until he is added again
		std::vector<runtime::Ref<Player>> playersToAdd;
		collectPlayersToAdd(inviter, invited, playersToAdd, alliance);

		if (!alliance) {
			alliance = PlayerAllianceService::createAlliance(inviter, invited, TeamType::ALLIANCE);
			// Java: List.remove(Object) - the first element equal to invited (AionObject.equals: the object id)
			auto it = std::ranges::find_if(playersToAdd, [&invited](const runtime::Ref<Player>& p) { return p->equals(invited); });
			if (it != playersToAdd.end())
				playersToAdd.erase(it);
		}

		for (const runtime::Ref<Player>& member : playersToAdd) {
			PlayerAllianceService::addPlayer(*alliance, *member);
		}
	}
}

void PlayerAllianceInvite::collectPlayersToAdd(Player& inviter, Player& invited, std::vector<runtime::Ref<Player>>& playersToAdd,
	runtime::Ptr<PlayerAlliance> alliance) {
	// Collect requester Group without leader
	if (inviter.isInGroup()) {
		if (alliance)
			throw commons::utils::IllegalArgumentException("If requester is in group, alliance should be null");
		runtime::Ref<PlayerGroup> group(*inviter.getPlayerGroup());
		const auto allExcept = utils::collections::Predicates::Players::allExcept(inviter);
		for (const runtime::Ptr<gameobjects::AionObject>& member :
			group->filterMembers([&allExcept](gameobjects::AionObject& object) { return allExcept(*runtime::cast<Player>(object)); }))
			playersToAdd.emplace_back(runtime::cast<Player>(member));

		for (const runtime::Ptr<gameobjects::AionObject>& player : group->getMembers())
			PlayerGroupService::removePlayer(*runtime::cast<Player>(player));
	}

	// Collect full Invited Group
	if (invited.isInGroup()) {
		runtime::Ref<PlayerGroup> group(*invited.getPlayerGroup());
		for (const runtime::Ptr<gameobjects::AionObject>& member : group->getMembers())
			playersToAdd.emplace_back(runtime::cast<Player>(member));
		for (const runtime::Ptr<gameobjects::AionObject>& player : group->getMembers())
			PlayerGroupService::removePlayer(*runtime::cast<Player>(player));
	} else { // or just single player
		playersToAdd.emplace_back(invited);
	}
}

void PlayerAllianceInvite::denyRequest(runtime::Ptr<gameobjects::Creature> requesterValue, Player& responder) {
	utils::PacketSendUtility::sendPacket(*runtime::cast<Player>(requesterValue),
		network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_PARTY_ALLIANCE_HE_REJECT_INVITATION(responder.getName()));
}

} // namespace aion::gameserver::model::team::alliance::events
