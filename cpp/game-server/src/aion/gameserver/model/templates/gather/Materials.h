#pragma once

#include "aion/gameserver/model/templates/gather/Materials.xml.h"

namespace aion::gameserver::model::templates::gather {

/** Java com.aionemu.gameserver.model.templates.gather.Materials. @author ATracer */
class Materials : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/gather/Materials.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::gather
