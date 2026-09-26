#pragma once

#include <vector>

#include "aion/gameserver/model/templates/globaldrops/GlobalDropRatings.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/** Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropRatings. @author AionCool */
class GlobalDropRatings : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropRatings.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<GlobalDropRating>& getGlobalDropRatings() const { return gdRatings; }
};

} // namespace aion::gameserver::model::templates::globaldrops
