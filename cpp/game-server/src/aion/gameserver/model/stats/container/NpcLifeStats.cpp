#include "aion/gameserver/model/stats/container/NpcLifeStats.h"

#include "aion/gameserver/services/LifeStatsRestoreService.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"

namespace aion::gameserver::model::stats::container {

NpcLifeStats::NpcLifeStats(gameobjects::Npc& ownerValue)
	: CreatureLifeStats(ownerValue, ownerValue.getGameStats()->getMaxHp()->getCurrent(), ownerValue.getGameStats()->getMaxMp()->getCurrent()) {
}

NpcLifeStats::~NpcLifeStats() = default;

void NpcLifeStats::triggerRestoreTask() {
	SYNCHRONIZED(restoreLock) {
		// lockdep: lifeRestoreTask.get() reads the Field<FutureRef>; nothing waits for the task
		if (!lifeRestoreTask.get() && !isDead()) {
			this->lifeRestoreTask = services::LifeStatsRestoreService::getInstance().scheduleHpRestoreTask(*this);
		}
	}
}

gameobjects::Npc& NpcLifeStats::getOwner() const {
	return static_cast<gameobjects::Npc&>(CreatureLifeStats::getOwner());
}

} // namespace aion::gameserver::model::stats::container
