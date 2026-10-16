#include "aion/gameserver/controllers/attack/AttackResult.h"

#include "aion/gameserver/model/templates/detail/JavaCasts.h"

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
	// Java: (int) damage - NaN becomes 0, out-of-range values saturate
	return model::templates::detail::floatToInt(damage.get());
}

void AttackResult::setShieldType(int32_t value) {
	// Java: this.shieldType |= shieldType (the setter accumulates the shield flags)
	shieldType.set(shieldType.get() | value);
}

AttackResult::~AttackResult() = default;

} // namespace aion::gameserver::controllers::attack
