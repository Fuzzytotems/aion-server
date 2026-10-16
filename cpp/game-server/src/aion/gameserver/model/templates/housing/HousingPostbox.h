#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingPostbox.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingPostbox. @author Rolandas */
class HousingPostbox : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingPostbox.xml.inc"
public:
	/** Java @Override getTypeId() (C++: non-virtual, see PlaceableHouseObject::getTypeId) */
	int8_t getTypeId() const { return 3; }
};

} // namespace aion::gameserver::model::templates::housing
