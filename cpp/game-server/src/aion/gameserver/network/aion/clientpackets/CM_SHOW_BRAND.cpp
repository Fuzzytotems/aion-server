#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_BRAND.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_BRAND.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_SHOW_BRAND::CM_SHOW_BRAND(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

using model::gameobjects::player::Player;

// Java CM_SHOW_BRAND.java:31-35
void CM_SHOW_BRAND::readImpl() {
	action = readD();
	brandId = readD();
	targetObjectId = readD();
}

// Java CM_SHOW_BRAND.java:38-46
void CM_SHOW_BRAND::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	runtime::Ptr<model::team::TemporaryPlayerTeam> team = player->getCurrentTeam();
	if (!team) {
		utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_SHOW_BRAND(brandId, targetObjectId));
	} else {
		runtime::Ptr<model::team::alliance::PlayerAlliance> alliance = runtime::as<model::team::alliance::PlayerAlliance>(team);
		if (team->isLeader(*player) || (alliance && alliance->isSomeCaptain(*player)))
			team->updateBrand(brandId, targetObjectId);
	}
}

AION_CLIENT_PACKET(CM_SHOW_BRAND);

} // namespace aion::gameserver::network::aion::clientpackets
