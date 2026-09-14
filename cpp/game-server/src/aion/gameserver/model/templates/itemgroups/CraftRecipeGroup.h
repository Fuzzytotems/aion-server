#pragma once

#include "aion/gameserver/model/templates/itemgroups/CraftRecipeGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.CraftRecipeGroup. @author Rolandas */
class CraftRecipeGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/CraftRecipeGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
