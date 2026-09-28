#include "aion/gameserver/questEngine/handlers/models/KillSpawnedData.h"

#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::questEngine::handlers::models {

void KillSpawnedData::register_(QuestEngine& /*questEngine*/) const {
	AION_UNPORTED();
}

std::optional<std::unordered_set<int32_t>> KillSpawnedData::getAlternativeNpcs(int32_t npcId) const {
	for (const Monster& m : monster) {
		if (auto others = otherNpcIds(m.getNpcIds(), npcId))
			return others;
	}
	return MonsterHuntData::getAlternativeNpcs(npcId);
}

} // namespace aion::gameserver::questEngine::handlers::models
