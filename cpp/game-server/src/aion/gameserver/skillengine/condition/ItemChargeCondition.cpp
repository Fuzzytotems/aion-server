#include "aion/gameserver/skillengine/condition/ItemChargeCondition.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::Item;
using runtime::Ptr;

bool ItemChargeCondition::validate(gameserver::model::stats::calc::Stat2& /*stat*/,
	gameserver::model::stats::calc::functions::IStatFunction& statFunction) const {
	Ptr<gameserver::model::stats::calc::StatOwner> owner = statFunction.getOwner();
	if (Ptr<Item> item = runtime::as<Item>(owner)) {
		return item->getChargeLevel() >= value;
	}
	return false;
}

bool ItemChargeCondition::validate(model::Skill& /*env*/) const {
	return false;
}

} // namespace aion::gameserver::skillengine::condition
