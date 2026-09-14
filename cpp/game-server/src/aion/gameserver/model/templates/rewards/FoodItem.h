#pragma once

#include "aion/gameserver/model/templates/rewards/FoodItem.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.FoodItem. */
class FoodItem : public ::aion::gameserver::model::templates::rewards::IdLevelReward {
#include "aion/gameserver/model/templates/rewards/FoodItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::rewards
