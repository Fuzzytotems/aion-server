#pragma once

#include "aion/gameserver/model/templates/itemgroups/OreGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.OreGroup. @author Rolandas */
class OreGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/OreGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
