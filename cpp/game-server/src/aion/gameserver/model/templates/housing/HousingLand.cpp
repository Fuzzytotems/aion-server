#include "aion/gameserver/model/templates/housing/HousingLand.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::housing {

const Building* HousingLand::getDefaultBuilding() const {
	if (!buildings) // Java: NullPointerException on the null list (a required wrapper, always bound)
		throw runtime::NullPointerException("HousingLand " + std::to_string(id) + " has no buildings");
	for (const Building& building : *buildings) {
		if (building.isDefault())
			return &building;
	}
	if (buildings->empty()) // fail
		throw runtime::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
	return &buildings->front();
}

} // namespace aion::gameserver::model::templates::housing
