#pragma once

#include "aion/gameserver/model/templates/itemgroups/EnchantGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.EnchantGroup. @author Rolandas */
class EnchantGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/EnchantGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
