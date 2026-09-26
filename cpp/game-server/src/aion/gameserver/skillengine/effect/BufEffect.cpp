#include "aion/gameserver/skillengine/effect/BufEffect.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/change/Change.h"
#include "aion/gameserver/skillengine/change/Func.h"
#include "aion/gameserver/skillengine/condition/Conditions.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

namespace functions = gameserver::model::stats::calc::functions;

using runtime::Ptr;
using runtime::Ref;

void BufEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void BufEffect::startEffect(model::Effect& effect) const {
	Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	Ptr<gameserver::model::stats::container::CreatureGameStats> cgs = effected->getGameStats();

	std::vector<Ref<functions::IStatFunction>> modifiersValue = getModifiers(effect); // Java: modifiers (the name of the template's member)

	if (modifiersValue.size() > 0) {
		// the stat functions keep their references: addEffect stores each one (wrapped in the effect's StatFunctionProxy)
		std::vector<Ptr<functions::IStatFunction>> borrowed(modifiersValue.begin(), modifiersValue.end());
		cgs->addEffect(Ptr<gameserver::model::stats::calc::StatOwner>(static_cast<gameserver::model::stats::calc::StatOwner&>(effect)), borrowed);
	}

	if (maxstat)
		effected->getLifeStats()->synchronizeWithMaxStats();
}

std::vector<Ref<functions::IStatFunction>> BufEffect::getModifiers(model::Effect& effect) const {
	// Java also reads effect.getSkillId() here, for the warning of a change whose stat JAXB could not bind (see the loop)
	int32_t skillLvl = effect.getSkillLevel();

	std::vector<Ref<functions::IStatFunction>> modifiersValue; // Java: modifiers (the name of the template's member)

	// Java: `if (change == null) return modifiers;` - JAXB leaves the list null without a <change>, where the bound C++ list is empty, and the loop
	// below adds nothing for an empty list either

	for (const change::Change& changeItem : change) {
		// Java: `if (changeItem.getStat() == null)` logs "Skill stat has wrong name for skillid: <id>" and skips the change - JAXB's null for a stat
		// name it does not know. The C++ binder rejects an unknown enum constant when the data is loaded (XmlValues.h), so a bound Change always has
		// its stat and the arm cannot be reached (docs/deviations/P5-03.md).

		// Java int arithmetic: value + delta * skillLvl wraps
		int32_t valueWithDelta = static_cast<int32_t>(
			static_cast<uint32_t>(changeItem.getValue()) + static_cast<uint32_t>(changeItem.getDelta()) * static_cast<uint32_t>(skillLvl));

		const condition::Conditions* conditions = changeItem.getConditions();
		switch (changeItem.getFunc()) {
			case change::Func::ADD: {
				Ref<functions::RcStatFunction<functions::StatAddFunction>> function =
					functions::RcStatFunction<functions::StatAddFunction>::create(changeItem.getStat(), valueWithDelta, true);
				function->withConditions(conditions);
				modifiersValue.push_back(function);
				break;
			}
			case change::Func::PERCENT: {
				Ref<functions::RcStatFunction<functions::StatRateFunction>> function =
					functions::RcStatFunction<functions::StatRateFunction>::create(changeItem.getStat(), valueWithDelta, true);
				function->withConditions(conditions);
				modifiersValue.push_back(function);
				break;
			}
			case change::Func::REPLACE: {
				Ref<functions::RcStatFunction<functions::StatSetFunction>> function =
					functions::RcStatFunction<functions::StatSetFunction>::create(changeItem.getStat(), valueWithDelta);
				function->withConditions(conditions);
				modifiersValue.push_back(function);
				break;
			}
		}
	}
	return modifiersValue;
}

} // namespace aion::gameserver::skillengine::effect
