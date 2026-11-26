#pragma once

#include <vector>

#include "aion/gameserver/model/templates/quest/CollectItems.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.CollectItems. @author MrPoke */
class CollectItems : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/CollectItems.xml.inc"
public:
	/** Java: an empty list without collect items (List.of()); the C++ list always exists and is read-only */
	const std::vector<CollectItem>& getCollectItem() const { return collectItem; }
};

} // namespace aion::gameserver::model::templates::quest
