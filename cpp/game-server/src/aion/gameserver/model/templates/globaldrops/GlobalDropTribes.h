#pragma once

#include <vector>

#include "aion/gameserver/model/templates/globaldrops/GlobalDropTribes.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropTribes. @author AionCool */
class GlobalDropTribes : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropTribes.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<GlobalDropTribe>& getGlobalDropTribes() const { return gdTribes; }
};

} // namespace aion::gameserver::model::templates::globaldrops
