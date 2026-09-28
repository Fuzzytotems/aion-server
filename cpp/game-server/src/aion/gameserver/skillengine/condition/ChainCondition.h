#pragma once

#include "aion/gameserver/skillengine/condition/ChainCondition.xml.h"

#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.ChainCondition. @author ATracer, kecimis, Neon */
class ChainCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/ChainCondition.xml.inc"
public:
	using Condition::validate; // C++ name hiding: the overloads this class does not override

	bool validate(model::Skill& env) const override;

private:
	// header-request: m5b2-p2-2 (Java private ChainCondition.shouldReset, ChainCondition.java:50-65; additive, non-virtual)
	bool shouldReset(model::ChainSkills& chain, model::Skill& env) const;
};

} // namespace aion::gameserver::skillengine::condition
