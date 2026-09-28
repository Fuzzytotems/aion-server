#pragma once

#include "aion/gameserver/skillengine/condition/Conditions.xml.h"

#include <memory>
#include <vector>

#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::condition {

/**
 * Java com.aionemu.gameserver.skillengine.condition.Conditions.
 * <p>
 * C++ notes (P4-08): `getConditions()` returns the bound list, which is empty where Java's is null (Java then returns an empty list). The
 * validate/canValidate families are behaviour (P5-02a; header request m5b2-p2-1 declares them, Conditions.java:41-75).
 *
 * @author ATracer
 */
class Conditions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/condition/Conditions.xml.inc"
public:
	const std::vector<std::unique_ptr<Condition>>& getConditions() const { return conditions; }

	// header-request: m5b2-p2-1 (the four Java methods of Conditions.java:41-75, additive, non-virtual)
	bool validate(model::Skill& skill) const;

	bool canValidate(model::Skill& skill) const;

	bool validate(gameserver::model::stats::calc::Stat2& stat, gameserver::model::stats::calc::functions::IStatFunction& statFunction) const;

	bool validate(model::Effect& effect) const;
};

} // namespace aion::gameserver::skillengine::condition
