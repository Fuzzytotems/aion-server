#include "aion/gameserver/network/aion/clientpackets/CM_BREAK_WEAPONS.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/ArmsfusionService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BREAK_WEAPONS::CM_BREAK_WEAPONS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BREAK_WEAPONS.java:23-27
void CM_BREAK_WEAPONS::readImpl() {
	npcObjId = readD();
	weaponObjId = readD();
}

// Java CM_BREAK_WEAPONS.java:29-36
void CM_BREAK_WEAPONS::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player->isTargetingNpcWithFunction(npcObjId, model::DialogAction::DECOMPOUND_WEAPON))
		services::ArmsfusionService::breakWeapons(*player, weaponObjId);
	else
		utils::audit::AuditLogger::log(*player, "tried to defuse a weapon without targeting an armsfusion officer");
}

AION_CLIENT_PACKET(CM_BREAK_WEAPONS);

} // namespace aion::gameserver::network::aion::clientpackets
