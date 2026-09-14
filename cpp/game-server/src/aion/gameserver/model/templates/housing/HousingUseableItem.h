#pragma once

#include "aion/gameserver/model/templates/housing/HousingUseableItem.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingUseableItem. @author Rolandas */
class HousingUseableItem : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingUseableItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
