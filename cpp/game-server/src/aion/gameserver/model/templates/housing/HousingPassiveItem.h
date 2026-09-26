#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingPassiveItem.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingPassiveItem. @author Rolandas */
class HousingPassiveItem : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingPassiveItem.xml.inc"
public:
	/** Java @Override getTypeId() (C++: non-virtual, see PlaceableHouseObject::getTypeId) */
	int8_t getTypeId() const { return 0; }
};

} // namespace aion::gameserver::model::templates::housing
