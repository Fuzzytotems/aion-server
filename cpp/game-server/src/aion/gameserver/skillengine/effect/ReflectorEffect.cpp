#include "aion/gameserver/skillengine/effect/ReflectorEffect.h"

#include <cstdint>
#include <optional>

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

void ReflectorEffect::startEffect(model::Effect& effect) const {
	int32_t hit = addInt(hitvalue, mulInt(hitdelta, effect.getSkillLevel()));

	Ref<AttackShieldObserver> asObserver = AttackShieldObserver::create(hit, value, percent, false, effect, hitType, getType(), hitTypeProb, minradius,
		radius, std::nullopt, 0);

	effect.addObserver(*effect.getEffected(), *asObserver);
}

void ReflectorEffect::endEffect(model::Effect& /*effect*/) const {
	// Java: empty body
}

model::ShieldType ReflectorEffect::getType() const {
	return reflectType == 1 ? model::ShieldType::SKILL_REFLECTOR : model::ShieldType::REFLECTOR;
}

} // namespace aion::gameserver::skillengine::effect
