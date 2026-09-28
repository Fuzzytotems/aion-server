#include "aion/gameserver/skillengine/effect/BoostSkillCastingTimeEffect.h"

#include <optional>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/change/Change.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void BoostSkillCastingTimeEffect::calculate(model::Effect& effect) const {
	if (effect.getEffected()->isEnemy(*effect.getEffector())) {
		// Java `if (change != null)`: JAXB leaves the list null without a <change>, where the bound C++ list is empty, and the loop below finds no
		// negative change in an empty list either (docs/deviations/P5-03.md, the BufEffect.getModifiers row)
		for (const change::Change& c : change) {
			if (c.getValue() < 0) {
				EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::SLOW_RESISTANCE, std::nullopt);
				return;
			}
		}
	}
	BufEffect::calculate(effect);
}

} // namespace aion::gameserver::skillengine::effect
