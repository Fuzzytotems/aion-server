#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/MedicineGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.MedicineGroup. @author Rolandas */
class MedicineGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/MedicineGroup.xml.inc"
public:
	MedicineGroup() : BonusItemGroup(GroupClass::MEDICINE) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<rewards::MedicineItem>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
