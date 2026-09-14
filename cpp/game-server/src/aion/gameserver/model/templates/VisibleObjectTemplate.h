#pragma once

#include "aion/gameserver/model/templates/VisibleObjectTemplate.xml.h"

namespace aion::gameserver::model::templates {

/** Java com.aionemu.gameserver.model.templates.VisibleObjectTemplate. @author ATracer */
class VisibleObjectTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/VisibleObjectTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates
