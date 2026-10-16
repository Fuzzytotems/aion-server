#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingMoveableItem.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingMoveableItem. @author Rolandas */
class HousingMoveableItem : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingMoveableItem.xml.inc"
public:
	/** Java @Override getTypeId() (C++: non-virtual, see PlaceableHouseObject::getTypeId) */
	int8_t getTypeId() const { return 0; }
};

} // namespace aion::gameserver::model::templates::housing
