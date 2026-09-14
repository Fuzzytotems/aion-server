#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropRaces.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropRaces. @author AionCool */
class GlobalDropRaces : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropRaces.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::globaldrops
