#pragma once

#include "aion/gameserver/model/templates/rewards/CraftReward.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.CraftReward. @author Rolandas */
class CraftReward : public ::aion::gameserver::model::templates::itemgroups::ItemRaceEntry {
#include "aion/gameserver/model/templates/rewards/CraftReward.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::rewards
