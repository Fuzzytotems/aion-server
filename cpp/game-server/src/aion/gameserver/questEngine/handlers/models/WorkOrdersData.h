#pragma once

#include "aion/gameserver/questEngine/handlers/models/WorkOrdersData.xml.h"

#include <cstdint>
#include <optional>
#include <unordered_set>

#include "aion/gameserver/questEngine/fwd.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.WorkOrdersData. @author Mr. Poke, Bobobear, Pad */
class WorkOrdersData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/WorkOrdersData.xml.inc"
public:
	void register_(QuestEngine& questEngine) const override;

	std::optional<std::unordered_set<int32_t>> getAlternativeNpcs(int32_t npcId) const override;
};

} // namespace aion::gameserver::questEngine::handlers::models
