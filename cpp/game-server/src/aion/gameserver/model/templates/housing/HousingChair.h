#pragma once

#include "aion/gameserver/model/templates/housing/HousingChair.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingChair. @author Rolandas */
class HousingChair : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingChair.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
