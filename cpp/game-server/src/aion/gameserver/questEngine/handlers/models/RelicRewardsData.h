#pragma once

#include "aion/gameserver/questEngine/handlers/models/RelicRewardsData.xml.h"

#include <cstdint>
#include <optional>
#include <unordered_set>

#include "aion/gameserver/questEngine/fwd.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.RelicRewardsData. @author Bobobear, Pad */
class RelicRewardsData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/RelicRewardsData.xml.inc"
public:
	void register_(QuestEngine& questEngine) const override;

	std::optional<std::unordered_set<int32_t>> getAlternativeNpcs(int32_t npcId) const override;
};

} // namespace aion::gameserver::questEngine::handlers::models
