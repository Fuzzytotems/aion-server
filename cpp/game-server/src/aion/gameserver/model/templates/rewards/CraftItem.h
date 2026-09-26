#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/rewards/CraftItem.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.CraftItem. C++: overrides are non-virtual (ItemRaceEntry.h). @author Rolandas */
class CraftItem : public ::aion::gameserver::model::templates::rewards::CraftReward {
#include "aion/gameserver/model/templates/rewards/CraftItem.xml.inc"
	friend class ::aion::gameserver::model::templates::itemgroups::ItemRaceEntry; // dispatches to the non-virtual overrides

public:
	CraftItem() : CraftReward(EntryClass::CRAFT_ITEM) {}

	/** Java @Override getCount: a random count of 3 to 5 */
	int64_t getCount() const;

protected:
	/** Java @Override matchesQuest */
	bool matchesQuest(const QuestTemplate& questTemplate) const;
};

} // namespace aion::gameserver::model::templates::rewards
