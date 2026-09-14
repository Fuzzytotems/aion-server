#pragma once

#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.xml.h"

namespace aion::gameserver::model::templates::staticdoor {

/** Java com.aionemu.gameserver.model.templates.staticdoor.StaticDoorTemplate. @author Wakizashi */
class StaticDoorTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::staticdoor
