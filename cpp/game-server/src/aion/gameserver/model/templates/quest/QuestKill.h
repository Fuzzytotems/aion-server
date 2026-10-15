#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/quest/QuestKill.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.QuestKill. @author Rolandas */
class QuestKill : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/QuestKill.xml.inc"
public:
	/**
	 * @return the npcIds
	 * <p>
	 * C++: Java moves the bound npc_ids into the transient list npcIdSet on the first call (a write to the shared template) and returns that
	 * list on every call; the C++ getter returns the bound ids (empty without the attribute), which is the same content.
	 */
	const std::vector<int32_t>& getNpcIds() const;
};

} // namespace aion::gameserver::model::templates::quest
