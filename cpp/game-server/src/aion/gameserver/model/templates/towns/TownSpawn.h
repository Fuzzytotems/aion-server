#pragma once

#include "aion/gameserver/model/templates/towns/TownSpawn.xml.h"

namespace aion::gameserver::model::templates::towns {

/** Java com.aionemu.gameserver.model.templates.towns.TownSpawn. @author ViAl */
class TownSpawn : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/towns/TownSpawn.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::towns
