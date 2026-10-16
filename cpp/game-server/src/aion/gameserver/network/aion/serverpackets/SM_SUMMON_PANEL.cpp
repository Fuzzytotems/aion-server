#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_PANEL.h"

#include <unordered_set>

#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/SummonGameStats.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/utils/stats/CalculationType.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SUMMON_PANEL::SM_SUMMON_PANEL(model::gameobjects::Summon& summonValue)
	: AionServerPacket(opcodeOf<SM_SUMMON_PANEL>), summon(summonValue) {
}

SM_SUMMON_PANEL::~SM_SUMMON_PANEL() = default;

void SM_SUMMON_PANEL::writeImpl(AionConnection* con) {
	const std::unordered_set<utils::stats::CalculationType> DISPLAY{utils::stats::CalculationType::DISPLAY};
	writeD(summon->getObjectId());
	writeH(summon->getLevel());
	writeD(0); // unk
	writeD(0); // unk
	writeD(summon->getLifeStats()->getCurrentHp());
	writeD(summon->getGameStats()->getMaxHp()->getCurrent());
	writeD(summon->getGameStats()->getMainHandPAttack(DISPLAY)->getCurrent());
	writeD(summon->getGameStats()->getPDef()->getCurrent());
	writeD(summon->getGameStats()->getMDef()->getCurrent());
	writeH(0); // unk
	writeD(summon->getLiveTime()); // life time
}

} // namespace aion::gameserver::network::aion::serverpackets
