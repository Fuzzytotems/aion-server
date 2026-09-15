#include "aion/gameserver/questEngine/handlers/models/KillInWorldData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::questEngine::handlers::models {

void KillInWorldData::register_(QuestEngine& /*questEngine*/) const {
	AION_UNPORTED();
}

std::optional<std::unordered_set<int32_t>> KillInWorldData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	if (auto others = otherNpcIds(endNpcIds, npcId))
		return others;
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
