#include "aion/gameserver/controllers/TrapController.h"

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/services/summons/TrapService.h"

namespace aion::gameserver::controllers {

TrapController::TrapController() = default;

TrapController::~TrapController() = default;

void TrapController::onDie(model::gameobjects::Creature& lastAttacker) {
	services::summons::TrapService::unregisterTrap(getOwner().getObjectId());
	NpcController::onDie(lastAttacker);
}

void TrapController::onDelete() {
	services::summons::TrapService::unregisterTrap(getOwner().getObjectId());
	NpcController::onDelete();
}

} // namespace aion::gameserver::controllers
