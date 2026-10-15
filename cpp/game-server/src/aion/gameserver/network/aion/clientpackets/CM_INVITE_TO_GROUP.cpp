#include "aion/gameserver/network/aion/clientpackets/CM_INVITE_TO_GROUP.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/model/team/league/LeagueService.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_INVITE_TO_GROUP::CM_INVITE_TO_GROUP(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

using model::gameobjects::player::Player;
using serverpackets::SM_SYSTEM_MESSAGE;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_INVITE_TO_GROUP");

// Java CM_INVITE_TO_GROUP.java:30-33
void CM_INVITE_TO_GROUP::readImpl() {
	inviteType = readUC();
	playerName = readS();
}

// Java CM_INVITE_TO_GROUP.java:36-68
void CM_INVITE_TO_GROUP::runImpl() {
	const runtime::Ptr<Player> inviter = getConnection()->getActivePlayer();
	if (inviter->isDead()) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_PARTY_CANT_INVITE_WHEN_DEAD());
		return;
	}

	const runtime::Ptr<Player> invited = world::World::getInstance().getPlayer(utils::ChatUtil::getRealCharName(playerName));
	if (!invited) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
		return;
	}

	if (invited->getPlayerSettings()->isInDeniedStatus(model::gameobjects::player::DeniedStatus::GROUP)) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_REJECTED_INVITE_PARTY(invited->getName(true)));
		return;
	}

	switch (inviteType) {
		case 0:
			model::team::group::PlayerGroupService::inviteToGroup(*inviter, *invited);
			break;
		case 12: // 2.5
			model::team::alliance::PlayerAllianceService::inviteToAlliance(*inviter, *invited);
			break;
		case 28:
			model::team::league::LeagueService::inviteToLeague(*inviter, *invited);
			break;
		default:
			log.warn("Received unknown invite type from player " + inviter->getName() + ": " + std::to_string(inviteType));
			break;
	}
}

AION_CLIENT_PACKET(CM_INVITE_TO_GROUP);

} // namespace aion::gameserver::network::aion::clientpackets
