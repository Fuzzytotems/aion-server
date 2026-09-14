#pragma once

#include "aion/gameserver/model/templates/gather/Material.xml.h"

namespace aion::gameserver::model::templates::gather {

/** Java com.aionemu.gameserver.model.templates.gather.Material. @author ATracer */
class Material : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/gather/Material.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::gather
