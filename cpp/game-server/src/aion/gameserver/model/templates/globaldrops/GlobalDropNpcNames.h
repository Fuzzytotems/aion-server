#pragma once

#include <vector>

#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcNames.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropNpcNames. @author bobobear */
class GlobalDropNpcNames : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcNames.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<GlobalDropNpcName>& getGlobalDropNpcNames() const { return gdNpcNames; }

	/** C++ only: the modifiable list for the holder's post-processing before publication (GlobalDropData.processRules clears it) */
	std::vector<GlobalDropNpcName>& getGlobalDropNpcNames() { return gdNpcNames; }
};

} // namespace aion::gameserver::model::templates::globaldrops
