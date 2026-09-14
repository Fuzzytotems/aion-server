#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropRatings.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropRatings. @author AionCool */
class GlobalDropRatings : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropRatings.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::globaldrops
