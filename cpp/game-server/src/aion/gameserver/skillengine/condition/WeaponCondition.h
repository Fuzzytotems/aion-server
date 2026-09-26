#pragma once

#include "aion/gameserver/skillengine/condition/WeaponCondition.xml.h"

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
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

private:
	// header-request: m5b2-p2-3 (Java private WeaponCondition.isValidWeapon, WeaponCondition.java:44-51; additive, non-virtual). The parameter is
	// nullable: validate(Stat2, ...) passes stat.getOwner(), and Java's instanceof answers false for null
	bool isValidWeapon(runtime::Ptr<gameserver::model::gameobjects::Creature> creature) const;
};

} // namespace aion::gameserver::skillengine::condition
