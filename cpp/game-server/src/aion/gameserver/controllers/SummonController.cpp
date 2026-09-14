#include "aion/gameserver/controllers/SummonController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Summon.h"

namespace aion::gameserver::controllers {

SummonController::SummonController() = default;

SummonController::~SummonController() = default;

model::gameobjects::Summon& SummonController::getOwner() const {
	return static_cast<model::gameobjects::Summon&>(CreatureController::getOwner());
}

void SummonController::notKnow(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void SummonController::release(model::summons::UnsummonType unsummonType) {
	AION_UNPORTED();
}

void SummonController::restMode() {
	AION_UNPORTED();
}

void SummonController::setUnkMode() {
	AION_UNPORTED();
}

void SummonController::guardMode() {
	AION_UNPORTED();
}

void SummonController::attackMode(int32_t targetObjId) {
	AION_UNPORTED();
}

bool SummonController::canAttack(int32_t targetObjId) {
	AION_UNPORTED();
}

void SummonController::attackTarget(runtime::Ptr<model::gameobjects::Creature> target, int32_t time, bool skipChecks) {
	AION_UNPORTED();
}

void SummonController::onAttack(model::gameobjects::Creature& creature, runtime::Ptr<skillengine::model::Effect> effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG log,
	std::optional<attack::AttackStatus> attackStatus, std::optional<skillengine::model::HopType> hopType) {
	AION_UNPORTED();
}

void SummonController::onTargetChanged(runtime::Ptr<model::gameobjects::VisibleObject> oldTarget,
	runtime::Ptr<model::gameobjects::VisibleObject> newTarget) {
	AION_UNPORTED();
}

void SummonController::onDespawn() {
	AION_UNPORTED();
}

void SummonController::onDie(model::gameobjects::Creature& lastAttacker) {
	AION_UNPORTED();
}

void SummonController::useSkill(model::summons::SkillOrder& order) {
	AION_UNPORTED();
}

void SummonController::onStartMove() {
	AION_UNPORTED();
}

void SummonController::onStopMove() {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> SummonController::getMaster() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
