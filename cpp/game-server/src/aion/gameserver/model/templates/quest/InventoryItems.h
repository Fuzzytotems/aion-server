#pragma once

#include "aion/gameserver/model/templates/quest/InventoryItems.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.InventoryItems. @author Rolandas */
class InventoryItems : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/InventoryItems.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::quest
