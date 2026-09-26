#include "aion/gameserver/questEngine/handlers/models/XmlQuestData.h"

#include <string>

#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnTalkEvent.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::questEngine::handlers::models {

void XmlQuestData::register_(QuestEngine& /*questEngine*/) const {
	AION_UNPORTED();
}

std::optional<std::unordered_set<int32_t>> XmlQuestData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	if (auto others = otherNpcIds(endNpcIds, npcId))
		return others;
	for (const xmlQuest::events::OnTalkEvent& onTalkEvent : onTalkEvents) { // Java: if (onTalkEvents != null)
		if (auto others = otherNpcIds(onTalkEvent.getIds(), npcId))
			return others;
	}
	for (const xmlQuest::events::OnKillEvent& onKillEvent : onKillEvents) { // Java: if (onKillEvents != null)
		for (const Monster& monster : onKillEvent.getMonsters()) {
			if (!monster.getNpcIds()) // Java: NullPointerException on npcIds.size()
				throw runtime::NullPointerException("monster without npc_ids in quest " + std::to_string(id));
			if (auto others = otherNpcIds(*monster.getNpcIds(), npcId))
				return others;
		}
	}
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
