#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_PLAYER_SKILLS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GM_SHOW_PLAYER_SKILLS::SM_GM_SHOW_PLAYER_SKILLS(const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skillListValue)
	: AionServerPacket(opcodeOf<SM_GM_SHOW_PLAYER_SKILLS>), skillList(skillListValue.begin(), skillListValue.end()) {
}

SM_GM_SHOW_PLAYER_SKILLS::~SM_GM_SHOW_PLAYER_SKILLS() = default;

void SM_GM_SHOW_PLAYER_SKILLS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
