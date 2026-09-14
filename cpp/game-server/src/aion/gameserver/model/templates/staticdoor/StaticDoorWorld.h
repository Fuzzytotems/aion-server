#pragma once

#include "aion/gameserver/model/templates/staticdoor/StaticDoorWorld.xml.h"

namespace aion::gameserver::model::templates::staticdoor {

/** Java com.aionemu.gameserver.model.templates.staticdoor.StaticDoorWorld. @author xTz */
class StaticDoorWorld : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/staticdoor/StaticDoorWorld.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::staticdoor
