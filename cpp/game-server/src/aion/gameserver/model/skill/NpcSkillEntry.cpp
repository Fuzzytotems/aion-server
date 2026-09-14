#include "aion/gameserver/model/skill/NpcSkillEntry.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::skill {

NpcSkillEntry::NpcSkillEntry(int32_t skillIdValue, int32_t skillLevelValue) : SkillEntry(skillIdValue, skillLevelValue) {
}

NpcSkillEntry::~NpcSkillEntry() = default;

void NpcSkillEntry::setLastTimeUsed() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::skill
