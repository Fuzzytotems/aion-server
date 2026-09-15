#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/portal/InstanceExit.xml.h"

namespace aion::gameserver::model::templates::portal {

/** Java com.aionemu.gameserver.model.templates.portal.InstanceExit. @author xTz */
class InstanceExit : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/portal/InstanceExit.xml.inc"
public:
	/** Java returns the boxed Integer of the int field (never null) */
	int32_t getInstanceId() const { return instanceId; }
};

} // namespace aion::gameserver::model::templates::portal
