#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_TELEPORT_BACK.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_HOUSE_TELEPORT_BACK::CM_HOUSE_TELEPORT_BACK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_HOUSE_TELEPORT_BACK.java:21-23
void CM_HOUSE_TELEPORT_BACK::readImpl() {
	// Nothing to read
}

// Java CM_HOUSE_TELEPORT_BACK.java:26-34
void CM_HOUSE_TELEPORT_BACK::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player) // Java: NullPointerException on player.getBattleReturnCoords()
		throw runtime::NullPointerException("CM_HOUSE_TELEPORT_BACK without an active player");
	const runtime::Ptr<runtime::Array<float>> coords = player->getBattleReturnCoords();
	if (coords && player->getBattleReturnMap() != 0) {
		services::teleport::TeleportService::teleportTo(*player, player->getBattleReturnMap(), 1, (*coords)[0], (*coords)[1], (*coords)[2], int8_t{0},
			model::animations::TeleportAnimation::FADE_OUT_BEAM);

		player->setBattleReturnCoords(0, nullptr);
	}
}

AION_CLIENT_PACKET(CM_HOUSE_TELEPORT_BACK);

} // namespace aion::gameserver::network::aion::clientpackets
