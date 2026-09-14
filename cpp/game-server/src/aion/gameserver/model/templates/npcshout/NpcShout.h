#pragma once

#include "aion/gameserver/model/templates/npcshout/NpcShout.xml.h"

namespace aion::gameserver::model::templates::npcshout {

/** Java com.aionemu.gameserver.model.templates.npcshout.NpcShout. @author Rolandas */
class NpcShout : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/npcshout/NpcShout.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::npcshout
