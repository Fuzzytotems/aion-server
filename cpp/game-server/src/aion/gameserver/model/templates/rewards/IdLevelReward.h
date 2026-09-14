#pragma once

#include "aion/gameserver/model/templates/rewards/IdLevelReward.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.IdLevelReward. @author Rolandas */
class IdLevelReward : public ::aion::gameserver::model::templates::itemgroups::ItemRaceEntry {
#include "aion/gameserver/model/templates/rewards/IdLevelReward.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::rewards
