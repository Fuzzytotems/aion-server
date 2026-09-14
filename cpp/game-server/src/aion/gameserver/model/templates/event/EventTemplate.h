#pragma once

#include "aion/gameserver/model/templates/event/EventTemplate.xml.h"

namespace aion::gameserver::model::templates::event {

/** Java com.aionemu.gameserver.model.templates.event.EventTemplate. @author Rolandas, Neon */
class EventTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/event/EventTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::event
