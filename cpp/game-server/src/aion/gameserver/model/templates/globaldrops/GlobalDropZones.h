#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropZones.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropZones. @author AionCool */
class GlobalDropZones : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropZones.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::globaldrops
