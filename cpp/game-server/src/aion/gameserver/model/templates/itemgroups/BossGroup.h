#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/BossGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.BossGroup. @author Rolandas */
class BossGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/BossGroup.xml.inc"
public:
	BossGroup() : BonusItemGroup(GroupClass::BOSS) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<ItemRaceEntry>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
