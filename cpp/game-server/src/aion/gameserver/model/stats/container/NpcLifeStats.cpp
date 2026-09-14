#include "aion/gameserver/model/stats/container/NpcLifeStats.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"

namespace aion::gameserver::model::stats::container {

NpcLifeStats::NpcLifeStats(gameobjects::Npc& ownerValue)
	: CreatureLifeStats(ownerValue, ownerValue.getGameStats()->getMaxHp()->getCurrent(), ownerValue.getGameStats()->getMaxMp()->getCurrent()) {
}

NpcLifeStats::~NpcLifeStats() = default;

void NpcLifeStats::triggerRestoreTask() {
	AION_UNPORTED();
}

gameobjects::Npc& NpcLifeStats::getOwner() const {
	return static_cast<gameobjects::Npc&>(CreatureLifeStats::getOwner());
}

} // namespace aion::gameserver::model::stats::container
