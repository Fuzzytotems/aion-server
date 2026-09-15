#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_REMOVE.h"

#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SKILL_REMOVE::SM_SKILL_REMOVE(model::skill::PlayerSkillEntry& skill)
	: AionServerPacket(opcodeOf<SM_SKILL_REMOVE>) {
	skillId = skill.getSkillId();
	skillLevel = skill.isProfessionSkill() ? skill.getProfessionFlag() : skill.getSkillLevel();
	skillType = skill.getSkillType();
}

void SM_SKILL_REMOVE::writeImpl(AionConnection* con) {
	writeH(skillId);
	writeC(skillLevel); // for professions, the getProfessionFlag() sent in SM_SKILL_LIST value is relevant, otherwise client won't remove it...
	writeC(skillType);
}

} // namespace aion::gameserver::network::aion::serverpackets
