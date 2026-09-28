#include "aion/gameserver/skillengine/effect/MPShieldEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/observer/AttackShieldObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
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

void MPShieldEffect::startEffect(model::Effect& effect) const {
	int32_t valueWithDelta = calculateBaseValue(effect);
	int32_t hitValueWithDelta = addInt(hitvalue, mulInt(hitdelta, effect.getSkillLevel()));
	// Stored in the effected's ObserveController and, through the removal task of Effect.addObserver, in the effect's observerRemoveTasks;
	// Effect.endEffect -> removeObservers removes it from both (the shield observer of ShieldEffect.startEffect, with the MP share on top)
	Ref<AttackShieldObserver> asObserver =
		AttackShieldObserver::create(hitValueWithDelta, valueWithDelta, percent, effect, hitType, getType(), hitTypeProb, mpValue);
	effect.addObserver(*effect.getEffected(), *asObserver);
}

void MPShieldEffect::endEffect(model::Effect& /*effect*/) const {
}

model::ShieldType MPShieldEffect::getType() const {
	return model::ShieldType::MPSHIELD;
}

} // namespace aion::gameserver::skillengine::effect
