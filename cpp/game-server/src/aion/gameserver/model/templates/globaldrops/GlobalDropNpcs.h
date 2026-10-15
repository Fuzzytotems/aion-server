#pragma once

#include <vector>

#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcs.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropNpcs. @author AionCool */
class GlobalDropNpcs : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcs.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<GlobalDropNpc>& getGlobalDropNpcs() const { return gdNpcs; }
};

} // namespace aion::gameserver::model::templates::globaldrops
