#include "aion/gameserver/network/aion/clientpackets/CM_FUSION_WEAPONS.h"

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

CM_FUSION_WEAPONS::CM_FUSION_WEAPONS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_FUSION_WEAPONS.java:24-29
void CM_FUSION_WEAPONS::readImpl() {
	npcObjId = readD();
	mainWeaponObjId = readD();
	fuseWeaponObjId = readD();
}

// Java CM_FUSION_WEAPONS.java:31-38
void CM_FUSION_WEAPONS::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player->isTargetingNpcWithFunction(npcObjId, model::DialogAction::COMPOUND_WEAPON))
		services::ArmsfusionService::fusionWeapons(*getConnection()->getActivePlayer(), mainWeaponObjId, fuseWeaponObjId);
	else
		utils::audit::AuditLogger::log(*player, "tried to fuse weapons without targeting an armsfusion officer");
}

AION_CLIENT_PACKET(CM_FUSION_WEAPONS);

} // namespace aion::gameserver::network::aion::clientpackets
