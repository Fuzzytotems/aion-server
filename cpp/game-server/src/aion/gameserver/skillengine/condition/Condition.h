#pragma once

#include "aion/gameserver/skillengine/condition/Condition.xml.h"

#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.Condition. @author ATracer */
class Condition : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/condition/Condition.xml.inc"
public:
	/**
	 * Validate condition specified in template
	 *
	 * @return true or false
	 */
	virtual bool validate(model::Skill& env) const = 0;

	/**
	 * Checks whether validate(Skill) would succeed, without paying anything.
	 *
	 * @return True, if the condition is met
	 */
	virtual bool canValidate(model::Skill& skill) const;

	/** Java: the StatCondition interface method (Condition is its only implementor, so C++ declares no StatCondition base) */
	virtual bool validate(gameserver::model::stats::calc::Stat2& stat,
		gameserver::model::stats::calc::functions::IStatFunction& statFunction) const;

	virtual bool validate(model::Effect& effect) const;
};

} // namespace aion::gameserver::skillengine::condition
