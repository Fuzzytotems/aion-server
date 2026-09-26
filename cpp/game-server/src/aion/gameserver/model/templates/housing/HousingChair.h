#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingChair.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingChair. @author Rolandas */
class HousingChair : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingChair.xml.inc"
public:
	/** Java @Override getTypeId() (C++: non-virtual, see PlaceableHouseObject::getTypeId) */
	int8_t getTypeId() const { return 5; }
};

} // namespace aion::gameserver::model::templates::housing
