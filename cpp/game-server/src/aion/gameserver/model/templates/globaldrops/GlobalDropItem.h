#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropItem.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropItem. @author AionCool */
class GlobalDropItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::globaldrops
