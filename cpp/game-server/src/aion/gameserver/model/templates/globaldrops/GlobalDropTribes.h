#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropTribes.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropTribes. @author AionCool */
class GlobalDropTribes : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropTribes.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::globaldrops
