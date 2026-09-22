#include "aion/gameserver/model/skill/NpcSkillEntry.h"

#include "aion/commons/utils/TimeUtils.h"

namespace aion::gameserver::model::skill {

NpcSkillEntry::NpcSkillEntry(int32_t skillIdValue, int32_t skillLevelValue) : SkillEntry(skillIdValue, skillLevelValue) {
}

NpcSkillEntry::~NpcSkillEntry() = default;

void NpcSkillEntry::setLastTimeUsed() {
	// Java: this.lastTimeUsed = System.currentTimeMillis() (NpcSkillEntry.java:35-37); the wall clock, as NpcSkillTemplateEntry::hasCooldown reads it
	lastTimeUsed.set(commons::utils::currentTimeMillis());
}

} // namespace aion::gameserver::model::skill
