#include "aion/gameserver/questEngine/handlers/models/ReportToManyData.h"

#include <string>

#include "aion/gameserver/questEngine/handlers/models/NpcInfos.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::questEngine::handlers::models {

void ReportToManyData::register_(QuestEngine& /*questEngine*/) const {
	AION_UNPORTED();
}

std::optional<std::unordered_set<int32_t>> ReportToManyData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	for (const NpcInfos& npcInfo : npcInfos) {
		if (!npcInfo.getNpcIds()) // Java: NullPointerException on npcIds.size() (npc_ids is required)
			throw runtime::NullPointerException("npc_infos without npc_ids in quest " + std::to_string(id));
		if (auto others = otherNpcIds(*npcInfo.getNpcIds(), npcId))
			return others;
	}
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
