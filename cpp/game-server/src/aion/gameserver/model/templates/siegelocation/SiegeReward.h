#pragma once

#include "aion/gameserver/model/templates/siegelocation/SiegeReward.xml.h"

namespace aion::gameserver::model::templates::siegelocation {

/** Java com.aionemu.gameserver.model.templates.siegelocation.SiegeReward. @author Source */
class SiegeReward : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/siegelocation/SiegeReward.xml.inc"
public:
	bool hasItemRewardsForWin() const { return itemId > 0 && itemCount > 0; }

	bool hasItemRewardsForDefeat() const { return itemIdDefeat > 0 && itemCountDefeat > 0; }
};

} // namespace aion::gameserver::model::templates::siegelocation
