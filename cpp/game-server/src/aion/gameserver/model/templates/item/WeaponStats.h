#pragma once

#include "aion/gameserver/model/templates/item/WeaponStats.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.WeaponStats. @author ATracer */
class WeaponStats : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/WeaponStats.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
