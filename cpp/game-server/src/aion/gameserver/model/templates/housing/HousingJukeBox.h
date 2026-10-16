#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingJukeBox.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingJukeBox. @author Rolandas */
class HousingJukeBox : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingJukeBox.xml.inc"
public:
	/** Java @Override getTypeId() (C++: non-virtual, see PlaceableHouseObject::getTypeId) */
	int8_t getTypeId() const { return 6; }
};

} // namespace aion::gameserver::model::templates::housing
