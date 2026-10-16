#include "aion/gameserver/model/skill/SkillEntry.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"

namespace aion::gameserver::model::skill {

SkillEntry::SkillEntry(int32_t skillIdValue, int32_t skillLevelValue) : skillId(skillIdValue), skillLevel(skillLevelValue) {
}

SkillEntry::~SkillEntry() = default;

const skillengine::model::SkillTemplate* SkillEntry::getSkillTemplate() {
	return dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
}

} // namespace aion::gameserver::model::skill
