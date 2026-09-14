#pragma once

#include "aion/gameserver/model/templates/portal/InstanceExit.xml.h"

namespace aion::gameserver::model::templates::portal {

/** Java com.aionemu.gameserver.model.templates.portal.InstanceExit. @author xTz */
class InstanceExit : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/portal/InstanceExit.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::portal
