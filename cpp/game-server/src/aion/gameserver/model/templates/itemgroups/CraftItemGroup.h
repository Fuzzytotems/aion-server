#pragma once

#include "aion/gameserver/model/templates/itemgroups/CraftItemGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.CraftItemGroup. @author Rolandas */
class CraftItemGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/CraftItemGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
