#pragma once

#include "aion/gameserver/model/templates/itemgroups/BonusItemGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.BonusItemGroup. @author Rolandas */
class BonusItemGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/itemgroups/BonusItemGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
