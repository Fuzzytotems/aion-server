#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_LIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SKILL_LIST::SM_SKILL_LIST(const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skillListValue)
	: AionServerPacket(opcodeOf<SM_SKILL_LIST>), skillList(skillListValue.begin(), skillListValue.end()), messageId(0), silentUpdate(true) {
}

SM_SKILL_LIST::SM_SKILL_LIST(model::skill::PlayerSkillEntry& skill, int32_t messageIdValue)
	: AionServerPacket(opcodeOf<SM_SKILL_LIST>), messageId(messageIdValue) {
	AION_UNPORTED();
}

SM_SKILL_LIST::~SM_SKILL_LIST() = default;

void SM_SKILL_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
