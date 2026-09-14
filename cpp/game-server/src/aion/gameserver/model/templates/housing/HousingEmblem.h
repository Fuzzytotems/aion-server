#pragma once

#include "aion/gameserver/model/templates/housing/HousingEmblem.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingEmblem. @author Rolandas */
class HousingEmblem : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingEmblem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
