#pragma once

#include "aion/gameserver/model/templates/housing/HouseAddress.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HouseAddress. @author Rolandas */
class HouseAddress : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/housing/HouseAddress.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
