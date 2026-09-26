#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author ATracer, xTz, Neon
 */
class SkillLearnService {
public:
	static void onLearnSkill(model::gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel, bool isNew);
private:
	static void sendPacket(model::gameobjects::player::Player& player, model::skill::PlayerSkillEntry& skill, bool isNew);
public:
	/** Adds all missing skills and recipes that can be auto-learned for the given level range. */
	static void learnNewSkills(model::gameobjects::player::Player& player, int32_t fromLevel, int32_t toLevel);
	static void learnTemporarySkill(model::gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel);
private:
	/** Adds auto-learned skills to the player, according to the specified level, class and race. */
	static void autoLearnSkills(model::gameobjects::player::Player& player, int32_t level, model::PlayerClass playerClass, model::Race playerRace);
public:
	static void learnSkillBook(model::gameobjects::player::Player& player, int32_t skillId);
	static bool removeSkill(model::gameobjects::player::Player& player, int32_t skillId);
};

} // namespace aion::gameserver::services
