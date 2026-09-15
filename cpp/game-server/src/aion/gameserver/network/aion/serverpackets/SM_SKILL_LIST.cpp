#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_LIST.h"

#include <string>

#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/skillinfo/SkillEntryWriter.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SKILL_LIST::SM_SKILL_LIST(const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skillListValue)
	: AionServerPacket(opcodeOf<SM_SKILL_LIST>), skillList(skillListValue.begin(), skillListValue.end()), messageId(0), silentUpdate(true) {
}

SM_SKILL_LIST::SM_SKILL_LIST(model::skill::PlayerSkillEntry& skill, int32_t messageIdValue)
	: AionServerPacket(opcodeOf<SM_SKILL_LIST>), messageId(messageIdValue) {
	skillList.emplace_back(skill); // Java: Collections.singletonList(skill)
	skillNameL10n = skill.getSkillTemplate()->getL10n();
	skillLvl = std::to_string(skill.getSkillLevel()); // Java String.valueOf(int)
}

SM_SKILL_LIST::~SM_SKILL_LIST() = default;

void SM_SKILL_LIST::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(skillList.size())); // skills list size
	writeC(silentUpdate ? 1 : 0); // 1 only list in skill list, 0 list in skill list, update skill bar level and notify if new
	for (const runtime::Ref<model::skill::PlayerSkillEntry>& entry : skillList)
		skillinfo::SkillEntryWriter::writeSkillEntry(*entry, getBuf());
	writeD(messageId);
	if (messageId != 0) {
		writeS(skillNameL10n);
		writeS(skillLvl);
		writeH(0x00);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
