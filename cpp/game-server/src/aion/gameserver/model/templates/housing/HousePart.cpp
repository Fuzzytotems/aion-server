#include "aion/gameserver/model/templates/housing/HousePart.h"

#include <string>

#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::housing {

bool HousePart::isForBuilding(const Building& building) const {
	if (!buildingTags)
		throw runtime::NullPointerException("HousePart " + std::to_string(id) + " has no building tags");
	return buildingTags->contains(building.getPartsMatchTag());
}

} // namespace aion::gameserver::model::templates::housing
