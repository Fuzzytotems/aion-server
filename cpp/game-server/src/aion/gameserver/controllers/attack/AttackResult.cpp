#include "aion/gameserver/controllers/attack/AttackResult.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::controllers::attack {

AttackResult::AttackResult(float value, AttackStatus attackStatusValue)
	: damage(value), attackStatus(attackStatusValue) {
}

runtime::Ref<AttackResult> AttackResult::create(float value, AttackStatus attackStatusValue) {
	return runtime::makeRef<AttackResult>(value, attackStatusValue);
}

AttackResult::AttackResult(float value, AttackStatus attackStatusValue, skillengine::model::HitType type)
	: AttackResult(value, attackStatusValue) {
	hitType.set(type);
}

runtime::Ref<AttackResult> AttackResult::create(float value, AttackStatus attackStatusValue, skillengine::model::HitType type) {
	return runtime::makeRef<AttackResult>(value, attackStatusValue, type);
}

int32_t AttackResult::getDamage() {
	AION_UNPORTED();
}

void AttackResult::setShieldType(int32_t value) {
	AION_UNPORTED();
}

AttackResult::~AttackResult() = default;

} // namespace aion::gameserver::controllers::attack
