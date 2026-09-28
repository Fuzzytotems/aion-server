#include "aion/gameserver/skillengine/effect/ShieldMasteryEffect.h"

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/stats/CalculationType.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;
using gameserver::model::stats::calc::Stat2;
using gameserver::model::stats::calc::functions::IStatFunction;
using gameserver::model::stats::calc::functions::StatRateFunction;
using gameserver::model::stats::container::StatEnum;
using runtime::Ptr;
using runtime::Ref;
using utils::stats::CalculationType;

namespace {

/**
 * Java com.aionemu.gameserver.model.stats.calc.functions.StatShieldMasteryFunction (StatShieldMasteryFunction.java:13-28, P5-01), ported
 * statement by statement in this file because the class has no C++ file: model/stats/calc/functions/fwd.h declares it and nothing defines it
 * (docs/deviations/P5-01.md "B-06 is deferred": its only constructor is this effect). Header request (P5-01): a declaration header and
 * StatShieldMasteryFunction.cpp in model/stats/calc/functions, to which this class moves unchanged (docs/deviations/P5-04.md). A run-time
 * StatFunction subclass, so it derives RefCounted itself and forwards IStatFunction's retain/release (StatFunction.h).
 *
 * @author VladimirZ
 */
// fieldmap-class: com.aionemu.gameserver.model.stats.calc.functions.StatShieldMasteryFunction
class StatShieldMasteryFunction final : public runtime::RefCounted, public StatRateFunction {
	AION_MAKE_REF_FRIEND
protected:
	StatShieldMasteryFunction(StatEnum name, int32_t valueValue, bool bonusValue) : StatRateFunction(name, valueValue, bonusValue) {}
	~StatShieldMasteryFunction() override = default;

public:
	/** Java: new StatShieldMasteryFunction(name, value, bonus) */
	static Ref<StatShieldMasteryFunction> create(StatEnum name, int32_t value, bool bonus) {
		return runtime::makeRef<StatShieldMasteryFunction>(name, value, bonus);
	}

	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& calculationTypes) override {
		Ptr<Player> player = runtime::cast<Player>(statValue.getOwner());
		if (player->getEquipment().isShieldEquipped())
			StatRateFunction::apply(statValue, calculationTypes);
	}
};

} // namespace

void ShieldMasteryEffect::startEffect(model::Effect& effect) const {
	std::vector<Ref<IStatFunction>> statModifiers = getModifiers(effect); // Java: modifiers (the name of an EffectTemplate member in C++)
	std::vector<Ref<IStatFunction>> masteryModifiers;
	for (const Ref<IStatFunction>& modifier : statModifiers) {
		masteryModifiers.push_back(StatShieldMasteryFunction::create(modifier->getName(), modifier->getValue(), modifier->isBonus()));
	}
	if (masteryModifiers.size() > 0) {
		// masteryModifiers holds the references until the stat container has taken its own (CreatureGameStats.addEffectOnly)
		effect.getEffected()->getGameStats()->addEffect(Ptr<gameserver::model::stats::calc::StatOwner>(effect),
			std::vector<Ptr<IStatFunction>>(masteryModifiers.begin(), masteryModifiers.end()));
	}
}

} // namespace aion::gameserver::skillengine::effect
