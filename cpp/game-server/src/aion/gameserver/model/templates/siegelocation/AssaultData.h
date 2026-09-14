#pragma once

#include "aion/gameserver/model/templates/siegelocation/AssaultData.xml.h"

namespace aion::gameserver::model::templates::siegelocation {

/** Java com.aionemu.gameserver.model.templates.siegelocation.AssaultData. @author Estrayl */
class AssaultData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/siegelocation/AssaultData.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::siegelocation
