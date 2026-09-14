#pragma once

#include "aion/gameserver/model/templates/housing/HousingJukeBox.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingJukeBox. @author Rolandas */
class HousingJukeBox : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingJukeBox.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
