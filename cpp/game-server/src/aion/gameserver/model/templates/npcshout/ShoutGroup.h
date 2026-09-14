#pragma once

#include "aion/gameserver/model/templates/npcshout/ShoutGroup.xml.h"

namespace aion::gameserver::model::templates::npcshout {

/** Java com.aionemu.gameserver.model.templates.npcshout.ShoutGroup. @author Rolandas */
class ShoutGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/npcshout/ShoutGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::npcshout
