#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/quest/Rewards.xml.h"

namespace aion::gameserver::model::templates::quest {

/**
 * Java com.aionemu.gameserver.model.templates.quest.Rewards.
 * <p>
 * C++: the list getters return the bound lists; an absent list is empty, which is what Java's `Collections.emptyList()` gives callers.
 */
class Rewards : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/Rewards.xml.inc"
public:
	const std::vector<QuestItems>& getSelectableRewardItem() const { return selectableRewardItem; }

	const std::vector<QuestItems>& getRewardItem() const { return rewardItem; }

	/** Java returns Collections.emptyList() without a ccheck attribute */
	const std::vector<int32_t>& getCollectItemChecks() const;
};

} // namespace aion::gameserver::model::templates::quest
