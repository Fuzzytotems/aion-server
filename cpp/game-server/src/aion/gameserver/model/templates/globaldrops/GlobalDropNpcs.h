#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcs.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropNpcs. @author AionCool */
class GlobalDropNpcs : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcs.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::globaldrops
