#pragma once

#include "aion/gameserver/model/templates/npcshout/ShoutList.xml.h"

namespace aion::gameserver::model::templates::npcshout {

/** Java com.aionemu.gameserver.model.templates.npcshout.ShoutList. @author Rolandas */
class ShoutList : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/npcshout/ShoutList.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::npcshout
