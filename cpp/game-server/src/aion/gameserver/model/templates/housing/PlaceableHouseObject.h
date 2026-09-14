#pragma once

#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.PlaceableHouseObject. @author Rolandas */
class PlaceableHouseObject : public ::aion::gameserver::model::templates::housing::AbstractHouseObject {
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
