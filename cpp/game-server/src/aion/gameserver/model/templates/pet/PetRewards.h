#pragma once

#include <vector>

#include "aion/gameserver/model/templates/pet/PetRewards.xml.h"

namespace aion::gameserver::model::templates::pet {

/** Java com.aionemu.gameserver.model.templates.pet.PetRewards. @author Rolandas */
class PetRewards : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/pet/PetRewards.xml.inc"
public:
	/** Java creates the list on first use; the C++ vector always exists */
	const std::vector<PetFeedResult>& getResults() const { return results; }
};

} // namespace aion::gameserver::model::templates::pet
