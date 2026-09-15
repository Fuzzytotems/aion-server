#pragma once

#include "aion/gameserver/questEngine/handlers/models/XmlQuestData.xml.h"

#include <cstdint>
#include <optional>
#include <unordered_set>

#include "aion/gameserver/questEngine/fwd.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.XmlQuestData. @author Mr. Poke, Pad */
class XmlQuestData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/XmlQuestData.xml.inc"
public:
	void register_(QuestEngine& questEngine) const override;

	std::optional<std::unordered_set<int32_t>> getAlternativeNpcs(int32_t npcId) const override;
};

} // namespace aion::gameserver::questEngine::handlers::models
