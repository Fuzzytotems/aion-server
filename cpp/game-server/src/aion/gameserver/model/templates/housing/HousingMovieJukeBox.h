#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingMovieJukeBox.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingMovieJukeBox. @author Rolandas */
class HousingMovieJukeBox : public ::aion::gameserver::model::templates::housing::HousingJukeBox {
#include "aion/gameserver/model/templates/housing/HousingMovieJukeBox.xml.inc"
public:
	/** Java @Override getTypeId() (C++: non-virtual, see PlaceableHouseObject::getTypeId) */
	int8_t getTypeId() const { return 0; } // unknown
};

} // namespace aion::gameserver::model::templates::housing
