#pragma once

#include "aion/gameserver/model/templates/housing/HousingNpc.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingNpc. @author Rolandas */
class HousingNpc : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingNpc.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
