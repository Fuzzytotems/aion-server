#include "aion/gameserver/skillengine/effect/ConvertHealEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/observer/AttackShieldObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::skillengine::effect {

using controllers::observer::AttackShieldObserver;

namespace {

/** Java int a * b (wraps on overflow), as ShieldEffect.cpp */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java int a + b (wraps on overflow), as ShieldEffect.cpp */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

} // namespace

void ConvertHealEffect::startEffect(model::Effect& effect) const {
	int32_t valueWithDelta = calculateBaseValue(effect);
	int32_t hitValueWithDelta = addInt(hitvalue, mulInt(hitdelta, effect.getSkillLevel()));
	runtime::Ref<AttackShieldObserver> asObserver = AttackShieldObserver::create(hitValueWithDelta, valueWithDelta, percent, hitPercent, effect, hitType,
		getType(), hitTypeProb, 0, 0, type, 0);
	effect.addObserver(*effect.getEffected(), *asObserver);
}

void ConvertHealEffect::endEffect(model::Effect& /*effect*/) const {
}

model::ShieldType ConvertHealEffect::getType() const {
	return model::ShieldType::CONVERT;
}

} // namespace aion::gameserver::skillengine::effect
