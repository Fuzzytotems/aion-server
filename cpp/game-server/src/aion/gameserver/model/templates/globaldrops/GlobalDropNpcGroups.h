#pragma once

#include <vector>

#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcGroups.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropNpcGroups. @author bobobear */
class GlobalDropNpcGroups : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcGroups.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<GlobalDropNpcGroup>& getGlobalDropNpcGroups() const { return gdNpcGroups; }
};

} // namespace aion::gameserver::model::templates::globaldrops
