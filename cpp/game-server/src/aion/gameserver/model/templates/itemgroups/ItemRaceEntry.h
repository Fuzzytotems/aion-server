#pragma once

#include "aion/gameserver/model/templates/itemgroups/ItemRaceEntry.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.ItemRaceEntry. @author Rolandas */
class ItemRaceEntry : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/itemgroups/ItemRaceEntry.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
