#include "aion/gameserver/model/team/group/events/PlayerGroupInvite.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::group::events {

using gameobjects::player::Player;

PlayerGroupInvite::PlayerGroupInvite(Player& inviter) : RequestResponseHandler(runtime::Ptr<gameobjects::Creature>(inviter)) {
}

PlayerGroupInvite::~PlayerGroupInvite() = default;

runtime::Ref<PlayerGroupInvite> PlayerGroupInvite::create(Player& inviter) {
	return runtime::makeRef<PlayerGroupInvite>(inviter);
}

void PlayerGroupInvite::acceptRequest(runtime::Ptr<gameobjects::Creature> requesterValue, Player& invited) {
	Player& inviter = *runtime::cast<Player>(requesterValue);
	if (restrictions::PlayerRestrictions::canInviteToGroup(inviter, invited)) {
		runtime::Ptr<PlayerGroup> group = inviter.getPlayerGroup();
		if (group) {
			PlayerGroupService::addPlayer(*group, invited);
		} else {
			PlayerGroupService::createGroup(inviter, invited, TeamType::GROUP, 0);
		}
	}
}

void PlayerGroupInvite::denyRequest(runtime::Ptr<gameobjects::Creature> requesterValue, Player& invited) {
	utils::PacketSendUtility::sendPacket(*runtime::cast<Player>(requesterValue),
		network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_PARTY_HE_REJECT_INVITATION(invited.getName()));
}

} // namespace aion::gameserver::model::team::group::events
