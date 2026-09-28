#include "aion/gameserver/questEngine/handlers/models/CraftingRewardsData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::questEngine::handlers::models {

void CraftingRewardsData::register_(QuestEngine& /*questEngine*/) const {
	AION_UNPORTED();
}

std::optional<std::unordered_set<int32_t>> CraftingRewardsData::getAlternativeNpcs(int32_t /*npcId*/) const {
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
