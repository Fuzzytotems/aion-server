#pragma once

#include "aion/gameserver/model/templates/gather/ExMaterials.xml.h"

namespace aion::gameserver::model::templates::gather {

/** Java com.aionemu.gameserver.model.templates.gather.ExMaterials. @author KID */
class ExMaterials : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/gather/ExMaterials.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::gather
