#pragma once

#include <vector>

#include "aion/gameserver/model/templates/ingameshop/IGCategory.xml.h"

namespace aion::gameserver::model::templates::ingameshop {

/** Java com.aionemu.gameserver.model.templates.ingameshop.IGCategory. */
class IGCategory : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/ingameshop/IGCategory.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<IGSubCategory>& getSubCategories() const { return subCategories; }
};

} // namespace aion::gameserver::model::templates::ingameshop
