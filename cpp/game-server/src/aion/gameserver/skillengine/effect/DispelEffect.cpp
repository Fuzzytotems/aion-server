#include "aion/gameserver/skillengine/effect/DispelEffect.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/EffectType.h"
#include "aion/gameserver/skillengine/model/DispelSlotType.h"
#include "aion/gameserver/skillengine/model/DispelType.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

namespace {

/** Java int a + b * c (wraps on overflow) */
constexpr int32_t addMulInt(int32_t a, int32_t b, int32_t c) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b) * static_cast<uint32_t>(c));
}

/** Java List<Integer>.get(index), unboxed: IndexOutOfBoundsException (JDK wording) outside [0, size) */
int32_t listGet(const std::vector<int32_t>& list, size_t index) {
	if (index >= list.size())
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(list.size()));
	return list[index];
}

} // namespace

void DispelEffect::applyEffect(model::Effect& effect) const {
	if (!effect.getEffected() || !effect.getEffected()->getEffectController())
		return;

	if (!dispeltype)
		return;

	// Java `effectids == null` (and `effecttype == null`, `slottype == null` below): JAXB leaves a list without elements null, and the bound C++
	// list is empty then; JAXB never produces an empty non-null list, so the two tests agree on every template (docs/deviations/P5-03.md)
	if ((*dispeltype == model::DispelType::EFFECTID || *dispeltype == model::DispelType::EFFECTIDRANGE) && effectids.empty())
		return;

	if (*dispeltype == model::DispelType::EFFECTTYPE && effecttype.empty())
		return;

	if (*dispeltype == model::DispelType::SLOTTYPE && slottype.empty())
		return;

	int32_t finalPower = addMulInt(power, dpower, effect.getSkillLevel());

	switch (*dispeltype) {
		case model::DispelType::EFFECTID: {
			int32_t removedEffects = 0;
			for (int32_t effectId : effectids) {
				if (removedEffects == count)
					break;
				if (effect.getEffected()->getEffectController()->removeByEffectId(effectId, dispelLevel, finalPower))
					removedEffects++;
			}
			break;
		}
		case model::DispelType::EFFECTIDRANGE: {
			int32_t removedEffectCount = 0;
			// Java re-reads effectids.get(1) at every test and increments an int (wrapping)
			for (int32_t effectId = listGet(effectids, 0); effectId <= listGet(effectids, 1);
				 effectId = static_cast<int32_t>(static_cast<uint32_t>(effectId) + 1u)) {
				if (removedEffectCount == count)
					break;
				if (effect.getEffected()->getEffectController()->removeByEffectId(effectId, dispelLevel, finalPower))
					removedEffectCount++;
			}
			break;
		}
		case model::DispelType::EFFECTTYPE:
			for (EffectType type : effecttype) {
				effect.getEffected()->getEffectController()->removeByDispelEffect(type, std::nullopt, count, dispelLevel, finalPower);
			}
			break;
		case model::DispelType::SLOTTYPE:
			for (model::DispelSlotType type : slottype) {
				effect.getEffected()->getEffectController()->removeByDispelEffect(std::nullopt, type, count, dispelLevel, finalPower);
			}
			break;
	}
}

} // namespace aion::gameserver::skillengine::effect
