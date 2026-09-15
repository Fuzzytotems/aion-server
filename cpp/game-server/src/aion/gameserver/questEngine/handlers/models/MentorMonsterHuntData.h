#pragma once

#include "aion/gameserver/questEngine/handlers/models/MentorMonsterHuntData.xml.h"

#include <cstdint>
#include <optional>
#include <unordered_set>

#include "aion/gameserver/questEngine/fwd.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.MentorMonsterHuntData. @author MrPoke, Bobobear, Pad */
class MentorMonsterHuntData : public ::aion::gameserver::questEngine::handlers::models::MonsterHuntData {
#include "aion/gameserver/questEngine/handlers/models/MentorMonsterHuntData.xml.inc"
public:
	void register_(QuestEngine& questEngine) const override;
};

} // namespace aion::gameserver::questEngine::handlers::models
