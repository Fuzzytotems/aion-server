#pragma once

#include "aion/gameserver/model/templates/quest/QuestWorkItems.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.QuestWorkItems. */
class QuestWorkItems : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/QuestWorkItems.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::quest
