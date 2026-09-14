#include "aion/gameserver/model/skill/SkillEntry.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::skill {

SkillEntry::SkillEntry(int32_t skillIdValue, int32_t skillLevelValue) : skillId(skillIdValue), skillLevel(skillLevelValue) {
}

SkillEntry::~SkillEntry() = default;

const skillengine::model::SkillTemplate* SkillEntry::getSkillTemplate() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::skill
