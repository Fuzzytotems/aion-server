#pragma once

#include "aion/gameserver/model/templates/spawns/TemporarySpawn.xml.h"

namespace aion::gameserver::model::templates::spawns {

/** Java com.aionemu.gameserver.model.templates.spawns.TemporarySpawn. @author xTz */
class TemporarySpawn : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/spawns/TemporarySpawn.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::spawns
