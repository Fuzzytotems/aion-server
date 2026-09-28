#pragma once

#include "aion/gameserver/model/templates/rewards/CraftReward.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.CraftReward. C++: overrides are non-virtual (ItemRaceEntry.h). @author Rolandas */
class CraftReward : public ::aion::gameserver::model::templates::itemgroups::ItemRaceEntry {
#include "aion/gameserver/model/templates/rewards/CraftReward.xml.inc"
	friend class ::aion::gameserver::model::templates::itemgroups::ItemRaceEntry; // dispatches to the non-virtual overrides

protected:
	/** C++ only: the constructor of CraftItem and CraftRecipe, naming their concrete class (Java's CraftReward is abstract) */
	explicit CraftReward(EntryClass value) noexcept : ItemRaceEntry(value) {}

	/** Java @Override matchesQuest */
	bool matchesQuest(const QuestTemplate& questTemplate) const;
};

} // namespace aion::gameserver::model::templates::rewards
