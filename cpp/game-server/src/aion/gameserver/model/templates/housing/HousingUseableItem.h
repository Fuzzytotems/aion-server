#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingUseableItem.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingUseableItem. @author Rolandas */
class HousingUseableItem : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingUseableItem.xml.inc"
public:
	/** Java @Override getTypeId() (C++: non-virtual, see PlaceableHouseObject::getTypeId) */
	int8_t getTypeId() const { return 1; }
};

} // namespace aion::gameserver::model::templates::housing
