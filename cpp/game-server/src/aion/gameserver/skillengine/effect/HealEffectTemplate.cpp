#include "aion/gameserver/skillengine/effect/HealEffectTemplate.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::stats::container::StatEnum;

namespace {

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java int a - b (wraps on overflow) */
constexpr int32_t subInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

} // namespace

int32_t HealEffectTemplate::calculateSnapshotHealValue(model::Effect& effect, model::HealType type) const {
	// Java int arithmetic: the product wraps, the division by 100 truncates towards zero (as C++ does)
	int32_t healValue = isPercent() ? mulInt(getMaxStatValue(effect), calculateBaseHealValue(effect)) / 100 : calculateBaseHealValue(effect);
	if (type == model::HealType::HP && allowHpHealBoost(effect)) {
		// caster's heal boost from equipment, titles, etc. (capped at 1000 / 100% boost)
		int32_t healBoost = effect.getEffector()->getGameStats()->getStat(StatEnum::HEAL_BOOST, 0)->getCurrent();
		// caster's heal related effects (passive boosts, active buffs e.g. blessed shield)
		int32_t healSkillBoost = subInt(effect.getEffector()->getGameStats()->getStat(StatEnum::HEAL_SKILL_BOOST, 1000)->getCurrent(), 1000);
		// Java: `healValue += (int) (healValue * Math.clamp(healBoost + healSkillBoost, 0, 2000) / 1000f)` - the int sum is clamped as a long, the
		// int product is divided as a float, and the compound assignment narrows the saturating (int) cast's result back into the int sum
		int32_t boost = static_cast<int32_t>(std::clamp<int64_t>(addInt(healBoost, healSkillBoost), 0, 2000));
		healValue = addInt(healValue, gameserver::model::templates::detail::floatToInt(static_cast<float>(mulInt(healValue, boost)) / 1000.0f));
	}
	return healValue;
}

int32_t HealEffectTemplate::applyHealDeboost(model::Effect& effect, int32_t healValue) const {
	// apply target's heal related effects (e.g. brilliant protection)
	if (allowHpHealSkillDeboost(effect))
		return std::max(0, effect.getEffected()->getGameStats()->getStat(StatEnum::HEAL_SKILL_DEBOOST, static_cast<float>(healValue))->getCurrent());
	return healValue;
}

int32_t HealEffectTemplate::calculateHealValue(model::Effect& effect, model::HealType type) const {
	int32_t healValue = calculateSnapshotHealValue(effect, type);
	if (type == model::HealType::HP)
		healValue = applyHealDeboost(effect, healValue);
	return healValue;
}

} // namespace aion::gameserver::skillengine::effect
