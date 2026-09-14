#pragma once

#include "aion/gameserver/model/templates/housing/HousingStorage.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingStorage. @author Rolandas */
class HousingStorage : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingStorage.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
