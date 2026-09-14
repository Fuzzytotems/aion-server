#pragma once

#include "aion/gameserver/model/templates/rewards/CraftItem.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.CraftItem. @author Rolandas */
class CraftItem : public ::aion::gameserver::model::templates::rewards::CraftReward {
#include "aion/gameserver/model/templates/rewards/CraftItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::rewards
