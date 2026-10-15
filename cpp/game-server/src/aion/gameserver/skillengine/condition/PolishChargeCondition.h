#pragma once

#include "aion/gameserver/skillengine/condition/PolishChargeCondition.xml.h"

#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.PolishChargeCondition. @author Rolandas, Cheatkiller */
class PolishChargeCondition : public ::aion::gameserver::skillengine::condition::ChargeCondition {
#include "aion/gameserver/skillengine/condition/PolishChargeCondition.xml.inc"
public:
	using ChargeCondition::validate; // C++ name hiding: the overloads this class does not override

	bool validate(model::Skill& env) const override;
};

} // namespace aion::gameserver::skillengine::condition
