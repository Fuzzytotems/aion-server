#include "aion/gameserver/skillengine/effect/ShieldEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/observer/AttackShieldObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::skillengine::effect {

using controllers::observer::AttackShieldObserver;
using runtime::Ref;

namespace {

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

} // namespace

void ShieldEffect::applyEffect(model::Effect& effect) const {
	// check for condition race, skillId: 10317,10318, implemented as RaceCondition
	effect.addToEffectedController();
}

void ShieldEffect::startEffect(model::Effect& effect) const {
	int32_t valueWithDelta = calculateBaseValue(effect);
	int32_t hitValueWithDelta = addInt(hitvalue, mulInt(hitdelta, effect.getSkillLevel()));

	Ref<AttackShieldObserver> asObserver =
		AttackShieldObserver::create(hitValueWithDelta, valueWithDelta, percent, effect, hitType, getType(), hitTypeProb);
	effect.addObserver(*effect.getEffected(), *asObserver);
}

model::ShieldType ShieldEffect::getType() const {
	return model::ShieldType::NORMAL;
}

} // namespace aion::gameserver::skillengine::effect
