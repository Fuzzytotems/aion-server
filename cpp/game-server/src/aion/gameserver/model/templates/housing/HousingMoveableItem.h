#pragma once

#include "aion/gameserver/model/templates/housing/HousingMoveableItem.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingMoveableItem. @author Rolandas */
class HousingMoveableItem : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingMoveableItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
