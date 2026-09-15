#pragma once

#include "aion/gameserver/skillengine/condition/ItemChargeCondition.xml.h"

#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.ItemChargeCondition. @author Rolandas */
class ItemChargeCondition : public ::aion::gameserver::skillengine::condition::ChargeCondition {
#include "aion/gameserver/skillengine/condition/ItemChargeCondition.xml.inc"
public:
	using ChargeCondition::validate; // C++ name hiding: the overloads this class does not override

	bool validate(gameserver::model::stats::calc::Stat2& stat,
		gameserver::model::stats::calc::functions::IStatFunction& statFunction) const override;

	bool validate(model::Skill& env) const override;
};

} // namespace aion::gameserver::skillengine::condition
