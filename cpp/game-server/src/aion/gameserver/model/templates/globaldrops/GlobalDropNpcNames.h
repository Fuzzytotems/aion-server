#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcNames.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropNpcNames. @author bobobear */
class GlobalDropNpcNames : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcNames.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::globaldrops
