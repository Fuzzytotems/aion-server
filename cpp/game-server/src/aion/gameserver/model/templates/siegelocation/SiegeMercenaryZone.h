#pragma once

#include "aion/gameserver/model/templates/siegelocation/SiegeMercenaryZone.xml.h"

namespace aion::gameserver::model::templates::siegelocation {

/** Java com.aionemu.gameserver.model.templates.siegelocation.SiegeMercenaryZone. @author Whoop */
class SiegeMercenaryZone : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/siegelocation/SiegeMercenaryZone.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::siegelocation
