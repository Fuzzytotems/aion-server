#include "aion/gameserver/controllers/movement/SiegeWeaponMoveController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Summon.h"

namespace aion::gameserver::controllers::movement {

SiegeWeaponMoveController::SiegeWeaponMoveController(model::gameobjects::Summon& ownerValue) : SummonMoveController(ownerValue) {
}

SiegeWeaponMoveController::~SiegeWeaponMoveController() = default;

void SiegeWeaponMoveController::moveToDestination() {
	AION_UNPORTED();
}

void SiegeWeaponMoveController::moveToTargetObject() {
	AION_UNPORTED();
}

void SiegeWeaponMoveController::abortMove() {
	AION_UNPORTED();
}

void SiegeWeaponMoveController::moveToLocation(float targetX, float targetY, float targetZ) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::movement
