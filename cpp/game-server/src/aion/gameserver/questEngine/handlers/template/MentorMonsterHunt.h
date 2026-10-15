#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/questEngine/handlers/template/MonsterHunt.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.MentorMonsterHunt: a monster_hunt that counts a kill only for a mentor with a mente
 * of the given levels in range, or for a mente with a mentor in range (the `mentor_monster_hunt` element, which no quest of the data uses).
 * <p>
 * C++: `menteMinLevel` and `menteMaxLevel` are effectively final in Java (assigned once by the constructor), so they are const here.
 *
 * @author MrPoke, Bobobear, Pad
 */
class MentorMonsterHunt : public MonsterHunt {
private:
	const int32_t menteMinLevel;
	const int32_t menteMaxLevel;

public:
	MentorMonsterHunt(int32_t questId, const std::optional<std::vector<int32_t>>& startNpcIds, const std::optional<std::vector<int32_t>>& endNpcIds,
		std::vector<models::Monster> monsters, int32_t menteMinLevel, int32_t menteMaxLevel, bool reward, bool rewardNextStep);

	bool onKillEvent(model::QuestEnv& env) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
