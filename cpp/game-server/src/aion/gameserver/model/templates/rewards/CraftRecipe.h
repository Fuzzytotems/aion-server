#pragma once

#include "aion/gameserver/model/templates/rewards/CraftRecipe.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.CraftRecipe. @author Rolandas */
class CraftRecipe : public ::aion::gameserver::model::templates::rewards::CraftReward {
#include "aion/gameserver/model/templates/rewards/CraftRecipe.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::rewards
