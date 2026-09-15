#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/EnchantGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.EnchantGroup. @author Rolandas */
class EnchantGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/EnchantGroup.xml.inc"
public:
	EnchantGroup() : BonusItemGroup(GroupClass::ENCHANT) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<rewards::IdLevelReward>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
