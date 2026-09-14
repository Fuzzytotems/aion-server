#pragma once

#include "aion/gameserver/model/templates/world/WorldMapTemplate.xml.h"

namespace aion::gameserver::model::templates::world {

/** Java com.aionemu.gameserver.model.templates.world.WorldMapTemplate. @author Luno */
class WorldMapTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/world/WorldMapTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::world
