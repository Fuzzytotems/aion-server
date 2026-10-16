#include "aion/gameserver/questEngine/handlers/models/Monster.h"

#include <algorithm>

namespace aion::gameserver::questEngine::handlers::models {

void Monster::addNpcIds(const std::vector<int32_t>& value) {
	if (!npcIds)
		npcIds.emplace();
	for (int32_t npc : value) {
		if (std::ranges::find(*npcIds, npc) == npcIds->end())
			npcIds->push_back(npc);
	}
}

} // namespace aion::gameserver::questEngine::handlers::models
