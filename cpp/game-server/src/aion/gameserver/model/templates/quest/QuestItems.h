#pragma once

#include "aion/gameserver/model/templates/quest/QuestItems.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.QuestItems. @author MrPoke */
class QuestItems : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/QuestItems.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::quest
