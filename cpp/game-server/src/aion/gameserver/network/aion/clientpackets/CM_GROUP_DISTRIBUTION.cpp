#include "aion/gameserver/network/aion/clientpackets/CM_GROUP_DISTRIBUTION.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/model/team/league/LeagueService.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_GROUP_DISTRIBUTION::CM_GROUP_DISTRIBUTION(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

using model::gameobjects::player::Player;

// Java CM_GROUP_DISTRIBUTION.java:29-32
void CM_GROUP_DISTRIBUTION::readImpl() {
	amount = readQ();
	partyType = readC();
}

// Java CM_GROUP_DISTRIBUTION.java:35-56
void CM_GROUP_DISTRIBUTION::runImpl() {
	if (amount < 2)
		return;
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (!restrictions::PlayerRestrictions::canTrade(*player))
		return;
	switch (partyType) {
		case 1:
			if (player->isInAlliance()) {
				model::team::alliance::PlayerAllianceService::distributeKinahInGroup(*player, amount);
			} else {
				model::team::group::PlayerGroupService::distributeKinah(*player, amount);
			}
			break;
		case 2:
			model::team::alliance::PlayerAllianceService::distributeKinah(*player, amount);
			break;
		case 3:
			model::team::league::LeagueService::distributeKinah(*player, amount);
			break;
	}
}

AION_CLIENT_PACKET(CM_GROUP_DISTRIBUTION);

} // namespace aion::gameserver::network::aion::clientpackets
