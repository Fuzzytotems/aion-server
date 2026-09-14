#pragma once

#include "aion/gameserver/model/templates/towns/TownSpawnMap.xml.h"

namespace aion::gameserver::model::templates::towns {

/** Java com.aionemu.gameserver.model.templates.towns.TownSpawnMap. @author ViAl */
class TownSpawnMap : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/towns/TownSpawnMap.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::towns
