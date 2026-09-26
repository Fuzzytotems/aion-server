#pragma once

#include <vector>

#include "aion/gameserver/model/templates/globaldrops/GlobalDropMaps.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropMaps. @author AionCool */
class GlobalDropMaps : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropMaps.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<GlobalDropMap>& getGlobalDropMaps() const { return gdMaps; }
};

} // namespace aion::gameserver::model::templates::globaldrops
