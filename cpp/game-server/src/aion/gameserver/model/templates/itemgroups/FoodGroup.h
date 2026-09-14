#pragma once

#include "aion/gameserver/model/templates/itemgroups/FoodGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.FoodGroup. @author Rolandas */
class FoodGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/FoodGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
