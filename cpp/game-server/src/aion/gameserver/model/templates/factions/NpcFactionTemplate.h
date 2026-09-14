#pragma once

#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.xml.h"

namespace aion::gameserver::model::templates::factions {

/** Java com.aionemu.gameserver.model.templates.factions.NpcFactionTemplate. @author vlog */
class NpcFactionTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::factions
