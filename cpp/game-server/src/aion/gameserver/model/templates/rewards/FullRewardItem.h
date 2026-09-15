#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/rewards/FullRewardItem.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.FullRewardItem. C++: overrides are non-virtual (ItemRaceEntry.h). @author Luzien, Pad */
class FullRewardItem : public ::aion::gameserver::model::templates::rewards::IdLevelReward {
#include "aion/gameserver/model/templates/rewards/FullRewardItem.xml.inc"
public:
	FullRewardItem() : IdLevelReward(EntryClass::FULL_REWARD_ITEM) {}

	/** Java @Override getCount */
	int64_t getCount() const { return count; }

	/** Java @Override getChance */
	float getChance() const { return chance; }
};

} // namespace aion::gameserver::model::templates::rewards
