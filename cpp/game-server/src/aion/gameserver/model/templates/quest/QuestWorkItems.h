#pragma once

#include "aion/gameserver/model/templates/quest/QuestWorkItems.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.QuestWorkItems. */
class QuestWorkItems : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/QuestWorkItems.xml.inc"
public:
	/** Java creates the list on first use; the C++ list always exists */
	const std::vector<QuestItems>& getQuestWorkItem() const { return questWorkItem; }
};

} // namespace aion::gameserver::model::templates::quest
