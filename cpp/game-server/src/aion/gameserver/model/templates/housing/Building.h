#pragma once

#include "aion/gameserver/model/templates/housing/Building.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.Building. @author Rolandas */
class Building : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/housing/Building.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
