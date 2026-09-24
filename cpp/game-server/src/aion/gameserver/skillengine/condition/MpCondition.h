#pragma once

#include "aion/gameserver/skillengine/condition/MpCondition.xml.h"

#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.MpCondition. @author ATracer */
class MpCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/MpCondition.xml.inc"
public:
	using Condition::validate; // C++ name hiding: the overloads this class does not override

	bool validate(model::Skill& env) const override;

	bool canValidate(model::Skill& skill) const override;

private:
	// header-request: m5b2-p2-3 (Java private MpCondition.getCost, MpCondition.java:48-57; additive, non-virtual)
	int32_t getCost(model::Skill& skill) const;
};

} // namespace aion::gameserver::skillengine::condition
