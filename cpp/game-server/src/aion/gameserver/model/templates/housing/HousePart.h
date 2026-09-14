#pragma once

#include "aion/gameserver/model/templates/housing/HousePart.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousePart. @author Rolandas */
class HousePart : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/housing/HousePart.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
