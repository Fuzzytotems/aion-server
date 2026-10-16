#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_PLAYER_SKILLS.h"

#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/skillinfo/SkillEntryWriter.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GM_SHOW_PLAYER_SKILLS::SM_GM_SHOW_PLAYER_SKILLS(const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skillListValue)
	: AionServerPacket(opcodeOf<SM_GM_SHOW_PLAYER_SKILLS>), skillList(skillListValue.begin(), skillListValue.end()) {
}

SM_GM_SHOW_PLAYER_SKILLS::~SM_GM_SHOW_PLAYER_SKILLS() = default;

void SM_GM_SHOW_PLAYER_SKILLS::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(skillList.size())); // skills list size
	for (const runtime::Ref<model::skill::PlayerSkillEntry>& entry : skillList)
		skillinfo::SkillEntryWriter::writeSkillEntry(*entry, getBuf());
}

} // namespace aion::gameserver::network::aion::serverpackets
