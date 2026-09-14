#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropMaps.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropMaps. @author AionCool */
class GlobalDropMaps : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropMaps.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::globaldrops
