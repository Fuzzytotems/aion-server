#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/event/EventQuestList.xml.h"

namespace aion::gameserver::model::templates::event {

/** Java com.aionemu.gameserver.model.templates.event.EventQuestList. @author Rolandas */
class EventQuestList : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/event/EventQuestList.xml.inc"
public:
	/** @return the startQuests (automatically started on logon); empty without the element */
	const std::vector<int32_t>& getStartableQuests() const;

	/** @return the maintainQuests (started indirectly from other quests); empty without the element */
	const std::vector<int32_t>& getMaintainQuests() const;
};

} // namespace aion::gameserver::model::templates::event
