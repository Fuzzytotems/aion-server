#include "aion/gameserver/skillengine/model/EffectReserved.h"

#include <functional>

#include "aion/gameserver/controllers/attack/AttackStatus.h"

namespace aion::gameserver::skillengine::model {

EffectReserved::EffectReserved(int32_t positionValue, int32_t valueValue, ResourceType typeValue, bool isDamageValue)
	: EffectReserved(positionValue, valueValue, typeValue, isDamageValue, true) {
}

EffectReserved::EffectReserved(int32_t positionValue, int32_t valueValue, ResourceType typeValue, bool isDamageValue, bool sendValue,
	controllers::attack::AttackStatus attackStatusValue)
	: position(positionValue), value(valueValue), type(typeValue), isDamage_(isDamageValue), send(sendValue), attackStatus(attackStatusValue) {
}

EffectReserved::EffectReserved(int32_t positionValue, int32_t valueValue, ResourceType typeValue, bool isDamageValue, bool sendValue)
	: EffectReserved(positionValue, valueValue, typeValue, isDamageValue, sendValue, controllers::attack::AttackStatus::NORMALHIT) {
}

EffectReserved::~EffectReserved() = default;

runtime::Ref<EffectReserved> EffectReserved::create(int32_t positionValue, int32_t valueValue, ResourceType typeValue, bool isDamageValue) {
	return runtime::makeRef<EffectReserved>(positionValue, valueValue, typeValue, isDamageValue);
}

runtime::Ref<EffectReserved> EffectReserved::create(int32_t positionValue, int32_t valueValue, ResourceType typeValue, bool isDamageValue,
	bool sendValue, controllers::attack::AttackStatus attackStatusValue) {
	return runtime::makeRef<EffectReserved>(positionValue, valueValue, typeValue, isDamageValue, sendValue, attackStatusValue);
}

runtime::Ref<EffectReserved> EffectReserved::create(int32_t positionValue, int32_t valueValue, ResourceType typeValue, bool isDamageValue,
	bool sendValue) {
	return runtime::makeRef<EffectReserved>(positionValue, valueValue, typeValue, isDamageValue, sendValue);
}

int32_t EffectReserved::getValueToSend() {
	if (isDamage_)
		return this->value;
	else
		return static_cast<int32_t>(0u - static_cast<uint32_t>(this->value)); // Java: -this.value (wraps for Integer.MIN_VALUE)
}

int32_t EffectReserved::compareTo(const EffectReserved& o) const {
	if (position < o.getPosition())
		return -1;
	if (position > o.getPosition())
		return 1;
	// Java: this.hashCode() - o.hashCode() (identity hash codes); C++: a consistent total order of distinct objects (see the class comment)
	if (this == &o)
		return 0;
	return std::less<const EffectReserved*>()(this, &o) ? -1 : 1;
}

} // namespace aion::gameserver::skillengine::model
