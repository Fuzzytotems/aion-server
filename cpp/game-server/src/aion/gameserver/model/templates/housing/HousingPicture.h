#pragma once

#include "aion/gameserver/model/templates/housing/HousingPicture.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingPicture. @author Rolandas */
class HousingPicture : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingPicture.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
