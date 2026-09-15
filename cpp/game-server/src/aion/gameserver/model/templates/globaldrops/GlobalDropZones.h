#pragma once

#include <vector>

#include "aion/gameserver/model/templates/globaldrops/GlobalDropZones.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropZones. @author AionCool */
class GlobalDropZones : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropZones.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<GlobalDropZone>& getGlobalDropZones() const { return gdZones; }
};

} // namespace aion::gameserver::model::templates::globaldrops
