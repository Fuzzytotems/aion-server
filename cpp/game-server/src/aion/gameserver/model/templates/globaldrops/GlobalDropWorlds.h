#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropWorlds.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropWorlds. @author AionCool */
class GlobalDropWorlds : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropWorlds.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::globaldrops
