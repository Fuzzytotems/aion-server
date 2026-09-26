#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/MedalGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.MedalGroup. @author Luzien */
class MedalGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/MedalGroup.xml.inc"
public:
	MedalGroup() : BonusItemGroup(GroupClass::MEDAL) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<rewards::FullRewardItem>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
