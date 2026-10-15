#pragma once

#include <vector>

#include "aion/gameserver/model/templates/quest/CollectItems.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.CollectItems. @author MrPoke */
class CollectItems : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/CollectItems.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists and is read-only */
	const std::vector<CollectItem>& getCollectItem() const { return collectItem; }

	/** @return false without a start_check attribute */
	bool getStartCheck() const { return startCheck.value_or(false); }
};

} // namespace aion::gameserver::model::templates::quest
