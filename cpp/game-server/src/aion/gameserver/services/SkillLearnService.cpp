#include "aion/gameserver/services/SkillLearnService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

void SkillLearnService::onLearnSkill(model::gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel, bool isNew) {
	AION_UNPORTED();
}

void SkillLearnService::sendPacket(model::gameobjects::player::Player& player, model::skill::PlayerSkillEntry& skill, bool isNew) {
	AION_UNPORTED();
}

void SkillLearnService::learnNewSkills(model::gameobjects::player::Player& player, int32_t fromLevel, int32_t toLevel) {
	AION_UNPORTED();
}

void SkillLearnService::learnTemporarySkill(model::gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel) {
	AION_UNPORTED();
}

void SkillLearnService::autoLearnSkills(model::gameobjects::player::Player& player, int32_t level, model::PlayerClass playerClass,
	model::Race playerRace) {
	AION_UNPORTED();
}

void SkillLearnService::learnSkillBook(model::gameobjects::player::Player& player, int32_t skillId) {
	AION_UNPORTED();
}

bool SkillLearnService::removeSkill(model::gameobjects::player::Player& player, int32_t skillId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
