#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingNpc.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingNpc. @author Rolandas */
class HousingNpc : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingNpc.xml.inc"
public:
	/** Java @Override getTypeId() (C++: non-virtual, see PlaceableHouseObject::getTypeId) */
	int8_t getTypeId() const { return 7; }
};

} // namespace aion::gameserver::model::templates::housing
