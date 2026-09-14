#pragma once

#include "aion/gameserver/model/templates/housing/HousingLand.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingLand. @author Rolandas */
class HousingLand : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/housing/HousingLand.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
