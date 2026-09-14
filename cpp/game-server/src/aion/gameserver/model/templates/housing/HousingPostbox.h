#pragma once

#include "aion/gameserver/model/templates/housing/HousingPostbox.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingPostbox. @author Rolandas */
class HousingPostbox : public ::aion::gameserver::model::templates::housing::PlaceableHouseObject {
#include "aion/gameserver/model/templates/housing/HousingPostbox.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
