#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/rewards/CraftRecipe.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.CraftRecipe. C++: overrides are non-virtual (ItemRaceEntry.h). @author Rolandas */
class CraftRecipe : public ::aion::gameserver::model::templates::rewards::CraftReward {
#include "aion/gameserver/model/templates/rewards/CraftRecipe.xml.inc"
	friend class ::aion::gameserver::model::templates::itemgroups::ItemRaceEntry; // dispatches to the non-virtual overrides

public:
	CraftRecipe() : CraftReward(EntryClass::CRAFT_RECIPE) {}

protected:
	/** Java @Override matchesQuest */
	bool matchesQuest(const QuestTemplate& questTemplate) const;

private:
	int32_t getMaxLevel() const;
};

} // namespace aion::gameserver::model::templates::rewards
