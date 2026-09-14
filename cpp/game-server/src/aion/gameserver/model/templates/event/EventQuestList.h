#pragma once

#include "aion/gameserver/model/templates/event/EventQuestList.xml.h"

namespace aion::gameserver::model::templates::event {

/** Java com.aionemu.gameserver.model.templates.event.EventQuestList. @author Rolandas */
class EventQuestList : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/event/EventQuestList.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::event
