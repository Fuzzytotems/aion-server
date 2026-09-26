#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/rewards/IdLevelReward.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.IdLevelReward. C++: overrides are non-virtual (ItemRaceEntry.h). @author Rolandas */
class IdLevelReward : public ::aion::gameserver::model::templates::itemgroups::ItemRaceEntry {
#include "aion/gameserver/model/templates/rewards/IdLevelReward.xml.inc"
	friend class ::aion::gameserver::model::templates::itemgroups::ItemRaceEntry; // dispatches to the non-virtual overrides

public:
	IdLevelReward() : ItemRaceEntry(EntryClass::ID_LEVEL_REWARD) {}

protected:
	/** C++ only: the constructor of FoodItem, MedicineItem and FullRewardItem, naming their concrete class */
	explicit IdLevelReward(EntryClass value) noexcept : ItemRaceEntry(value) {}

	/** Java @Override matchesLevel */
	bool matchesLevel(const item::ItemTemplate& itemTemplate, int32_t bonusItemLevel) const { return bonusItemLevel == 0 || level == bonusItemLevel; }
};

} // namespace aion::gameserver::model::templates::rewards
