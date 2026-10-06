#include "aion/gameserver/model/team/league/events/LeagueInviteEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/LeagueService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::league::events {

using gameobjects::player::Player;

LeagueInviteEvent::LeagueInviteEvent(Player& requester, Player& invitedValue)
	: RequestResponseHandler(runtime::Ptr<gameobjects::Creature>(requester)), invited(invitedValue) {
}

LeagueInviteEvent::~LeagueInviteEvent() = default;

runtime::Ref<LeagueInviteEvent> LeagueInviteEvent::create(Player& requester, Player& invitedValue) {
	return runtime::makeRef<LeagueInviteEvent>(requester, invitedValue);
}

void LeagueInviteEvent::acceptRequest(runtime::Ptr<gameobjects::Creature> requesterValue, Player& responder) {
	static_cast<void>(responder);
	Player& requesterPlayer = *runtime::cast<Player>(requesterValue); // Java: requester
	Player& invitedValue = *invited;
	if (LeagueService::canInvite(requesterPlayer, invitedValue)) {
		runtime::Ptr<alliance::PlayerAlliance> requesterAlliance = requesterPlayer.getPlayerAlliance();
		if (!requesterAlliance)
			throw runtime::NullPointerException("requester.getPlayerAlliance()");
		runtime::Ptr<League> league = requesterAlliance->getLeague();

		if (!league) {
			league = LeagueService::createLeague(requesterPlayer);
		}
		if (!invitedValue.isInLeague()) {
			runtime::Ptr<alliance::PlayerAlliance> invitedAlliance = invitedValue.getPlayerAlliance();
			if (!invitedAlliance)
				throw runtime::NullPointerException("invited.getPlayerAlliance()");
			LeagueService::addAlliance(*league, *invitedAlliance);
		}
	}
}

void LeagueInviteEvent::denyRequest(runtime::Ptr<gameobjects::Creature> requesterValue, Player& responder) {
	utils::PacketSendUtility::sendPacket(*runtime::cast<Player>(requesterValue),
		network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_PARTY_ALLIANCE_HE_REJECT_INVITATION(responder.getName()));
}

} // namespace aion::gameserver::model::team::league::events
