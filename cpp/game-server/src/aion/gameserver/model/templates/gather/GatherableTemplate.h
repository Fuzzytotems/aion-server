#pragma once

#include "aion/gameserver/model/templates/gather/GatherableTemplate.xml.h"

namespace aion::gameserver::model::templates::gather {

/** Java com.aionemu.gameserver.model.templates.gather.GatherableTemplate. @author ATracer, KID */
class GatherableTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/gather/GatherableTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::gather
