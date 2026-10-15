#pragma once

#include <vector>

#include "aion/gameserver/model/templates/globaldrops/GlobalDropRaces.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropRaces. @author AionCool */
class GlobalDropRaces : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropRaces.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<GlobalDropRace>& getGlobalDropRaces() const { return gdRaces; }
};

} // namespace aion::gameserver::model::templates::globaldrops
