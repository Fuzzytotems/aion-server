#pragma once

#include "aion/gameserver/model/templates/tribe/Tribe.xml.h"

namespace aion::gameserver::model::templates::tribe {

/** Java com.aionemu.gameserver.model.templates.tribe.Tribe. @author ATracer */
class Tribe : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/tribe/Tribe.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::tribe
