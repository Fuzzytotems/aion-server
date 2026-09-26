#pragma once

#include <vector>

#include "aion/gameserver/model/templates/globaldrops/GlobalDropWorlds.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropWorlds. @author AionCool */
class GlobalDropWorlds : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropWorlds.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<GlobalDropWorld>& getGlobalDropWorlds() const { return gdWorlds; }
};

} // namespace aion::gameserver::model::templates::globaldrops
