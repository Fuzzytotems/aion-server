#pragma once

#include "aion/gameserver/model/templates/quest/QuestDrop.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.QuestDrop. @author MrPoke */
class QuestDrop : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/QuestDrop.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::quest
