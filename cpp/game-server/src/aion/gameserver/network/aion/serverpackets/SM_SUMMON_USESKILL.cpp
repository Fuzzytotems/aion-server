#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_USESKILL.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SUMMON_USESKILL::SM_SUMMON_USESKILL(int32_t summonIdValue, int32_t skillIdValue, int32_t skillLvlValue, int32_t targetIdValue)
	: AionServerPacket(opcodeOf<SM_SUMMON_USESKILL>), summonId(summonIdValue), skillId(skillIdValue), skillLvl(skillLvlValue),
	  targetId(targetIdValue) {
}

void SM_SUMMON_USESKILL::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
