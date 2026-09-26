#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingStorage.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingStorage. @author Rolandas */
class HousingStorage : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingStorage.xml.inc"
public:
	/** Java @Override getTypeId() (C++: non-virtual, see PlaceableHouseObject::getTypeId) */
	int8_t getTypeId() const { return 2; }
};

} // namespace aion::gameserver::model::templates::housing
