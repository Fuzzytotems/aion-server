#pragma once

#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeLevels.xml.h"

namespace aion::gameserver::model::templates::event::upgradearcade {

/** Java com.aionemu.gameserver.model.templates.event.upgradearcade.ArcadeLevels. @author Neon */
class ArcadeLevels : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeLevels.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::event::upgradearcade
