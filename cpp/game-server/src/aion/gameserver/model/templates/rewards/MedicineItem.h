#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/rewards/MedicineItem.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.MedicineItem. C++: overrides are non-virtual (ItemRaceEntry.h). */
class MedicineItem : public ::aion::gameserver::model::templates::rewards::IdLevelReward {
#include "aion/gameserver/model/templates/rewards/MedicineItem.xml.inc"
public:
	MedicineItem() : IdLevelReward(EntryClass::MEDICINE_ITEM) {}

	/** Java @Override getCount: a random count of 1 to 3 */
	int64_t getCount() const;
};

} // namespace aion::gameserver::model::templates::rewards
