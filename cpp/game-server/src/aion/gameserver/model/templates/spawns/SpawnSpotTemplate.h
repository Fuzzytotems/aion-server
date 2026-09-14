#pragma once

#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.xml.h"

namespace aion::gameserver::model::templates::spawns {

/** Java com.aionemu.gameserver.model.templates.spawns.SpawnSpotTemplate. @author xTz, Rolandas */
class SpawnSpotTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::spawns
