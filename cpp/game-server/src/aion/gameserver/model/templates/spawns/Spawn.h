#pragma once

#include "aion/gameserver/model/templates/spawns/Spawn.xml.h"

namespace aion::gameserver::model::templates::spawns {

/** Java com.aionemu.gameserver.model.templates.spawns.Spawn. @author xTz, Rolandas */
class Spawn : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/spawns/Spawn.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::spawns
