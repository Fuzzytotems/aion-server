#pragma once

#include "aion/gameserver/model/templates/quest/QuestKill.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.QuestKill. @author Rolandas */
class QuestKill : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/QuestKill.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::quest
