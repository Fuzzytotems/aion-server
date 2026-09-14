#pragma once

#include "aion/gameserver/model/templates/siegelocation/DoorRepairData.xml.h"

namespace aion::gameserver::model::templates::siegelocation {

/** Java com.aionemu.gameserver.model.templates.siegelocation.DoorRepairData. */
class DoorRepairData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/siegelocation/DoorRepairData.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::siegelocation
