#pragma once

#include "aion/gameserver/model/templates/event/Buff.xml.h"

namespace aion::gameserver::model::templates::event {

/** Java com.aionemu.gameserver.model.templates.event.Buff. @author Neon */
class Buff : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/event/Buff.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::event
