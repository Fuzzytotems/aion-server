#include "aion/gameserver/controllers/SiegeWeaponController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Summon.h"

namespace aion::gameserver::controllers {

SiegeWeaponController::SiegeWeaponController(int32_t npcId) : skills(nullptr) {
	// Java: skills = DataManager.NPC_SKILL_DATA.getNpcSkillList(npcId)
	AION_UNPORTED();
}

SiegeWeaponController::~SiegeWeaponController() = default;

void SiegeWeaponController::release(model::summons::UnsummonType unsummonType) {
	AION_UNPORTED();
}

void SiegeWeaponController::restMode() {
	AION_UNPORTED();
}

void SiegeWeaponController::setUnkMode() {
	AION_UNPORTED();
}

void SiegeWeaponController::guardMode() {
	AION_UNPORTED();
}

void SiegeWeaponController::attackMode(int32_t targetObjId) {
	AION_UNPORTED();
}

bool SiegeWeaponController::isValidTarget(model::gameobjects::Creature& target) {
	AION_UNPORTED();
}

bool SiegeWeaponController::isBalaurBoss(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

void SiegeWeaponController::onDie(model::gameobjects::Creature& lastAttacker) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
