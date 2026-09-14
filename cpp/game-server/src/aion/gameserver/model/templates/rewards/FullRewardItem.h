#pragma once

#include "aion/gameserver/model/templates/rewards/FullRewardItem.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.FullRewardItem. @author Luzien, Pad */
class FullRewardItem : public ::aion::gameserver::model::templates::rewards::IdLevelReward {
#include "aion/gameserver/model/templates/rewards/FullRewardItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::rewards
