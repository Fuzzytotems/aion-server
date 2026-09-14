#pragma once

#include "aion/gameserver/model/templates/spawns/HouseSpawns.xml.h"

namespace aion::gameserver::model::templates::spawns {

/** Java com.aionemu.gameserver.model.templates.spawns.HouseSpawns. @author Rolandas */
class HouseSpawns : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/spawns/HouseSpawns.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::spawns
