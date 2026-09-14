#pragma once

#include "aion/gameserver/model/templates/spawns/SpawnMap.xml.h"

namespace aion::gameserver::model::templates::spawns {

/** Java com.aionemu.gameserver.model.templates.spawns.SpawnMap. @author xTz */
class SpawnMap : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/spawns/SpawnMap.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::spawns
