#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingEmblem.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingEmblem. @author Rolandas */
class HousingEmblem : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingEmblem.xml.inc"
public:
	/** Java @Override getTypeId() (C++: non-virtual, see PlaceableHouseObject::getTypeId) */
	int8_t getTypeId() const { return 11; }
};

} // namespace aion::gameserver::model::templates::housing
