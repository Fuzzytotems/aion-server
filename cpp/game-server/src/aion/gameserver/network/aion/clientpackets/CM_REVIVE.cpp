#include "aion/gameserver/network/aion/clientpackets/CM_REVIVE.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/ReviveType.h"
#include "aion/gameserver/model/gameobjects/player/ReviveTypeInfo.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using model::gameobjects::player::ReviveType;
using services::player::PlayerReviveService;

CM_REVIVE::CM_REVIVE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_REVIVE::readImpl() {
	reviveId = readUC();
}

void CM_REVIVE::runImpl() {
	runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();

	if (!activePlayer->isDead())
		return;

	// Java: ReviveType.getReviveTypeById(reviveId) throws IllegalArgumentException for an id the enum does not know; the packet boundary
	// logs it, as it does in Java (ReviveTypeInfo.h:34-42).
	ReviveType reviveType = model::gameobjects::player::getReviveTypeById(reviveId);

	switch (reviveType) {
		case ReviveType::BIND_REVIVE:
		case ReviveType::OBELISK_REVIVE:
			PlayerReviveService::bindRevive(*activePlayer);
			break;
		case ReviveType::REBIRTH_REVIVE:
			PlayerReviveService::rebirthRevive(*activePlayer);
			break;
		case ReviveType::ITEM_SELF_REVIVE:
			PlayerReviveService::itemSelfRevive(*activePlayer);
			break;
		case ReviveType::SKILL_REVIVE:
			PlayerReviveService::skillRevive(*activePlayer);
			break;
		case ReviveType::KISK_REVIVE:
			PlayerReviveService::kiskRevive(*activePlayer);
			break;
		case ReviveType::INSTANCE_REVIVE:
			PlayerReviveService::instanceRevive(*activePlayer);
			break;
	}
}

AION_CLIENT_PACKET(CM_REVIVE);

} // namespace aion::gameserver::network::aion::clientpackets
