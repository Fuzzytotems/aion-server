#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcGroups.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropNpcGroups. @author bobobear */
class GlobalDropNpcGroups : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcGroups.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::globaldrops
