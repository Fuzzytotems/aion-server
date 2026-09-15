#pragma once

#include "aion/gameserver/skillengine/condition/WeaponCondition.xml.h"

#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.WeaponCondition. @author ATracer */
class WeaponCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/WeaponCondition.xml.inc"
public:
	using Condition::validate; // C++ name hiding: the overloads this class does not override

	bool validate(model::Skill& env) const override;

	bool validate(gameserver::model::stats::calc::Stat2& stat,
		gameserver::model::stats::calc::functions::IStatFunction& statFunction) const override;
};

} // namespace aion::gameserver::skillengine::condition
