#pragma once

#include "aion/gameserver/model/templates/quest/CollectItems.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.CollectItems. @author MrPoke */
class CollectItems : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/CollectItems.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::quest
