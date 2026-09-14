#pragma once

#include "aion/gameserver/model/templates/housing/AbstractHouseObject.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.AbstractHouseObject. @author Rolandas */
class AbstractHouseObject : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/housing/AbstractHouseObject.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::housing
