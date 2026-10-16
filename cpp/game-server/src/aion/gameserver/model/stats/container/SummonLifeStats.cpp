#include "aion/gameserver/model/stats/container/SummonLifeStats.h"

#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/SummonGameStats.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/LifeStatsRestoreService.h"

namespace aion::gameserver::model::stats::container {

SummonLifeStats::SummonLifeStats(gameobjects::Summon& ownerValue)
	: CreatureLifeStats(ownerValue, ownerValue.getGameStats()->getMaxHp()->getCurrent(), ownerValue.getGameStats()->getMaxMp()->getCurrent()) {
}

SummonLifeStats::~SummonLifeStats() = default;

void SummonLifeStats::triggerRestoreTask() {
	SYNCHRONIZED(restoreLock) {
		// lockdep: lifeRestoreTask.get() reads the Field<FutureRef>; nothing waits for the task (as NpcLifeStats::triggerRestoreTask)
		if (!lifeRestoreTask.get() && !isDead())
			this->lifeRestoreTask = services::LifeStatsRestoreService::getInstance().scheduleHpRestoreTask(*this);
	}
}

gameobjects::Summon& SummonLifeStats::getOwner() const {
	return static_cast<gameobjects::Summon&>(CreatureLifeStats::getOwner());
}

} // namespace aion::gameserver::model::stats::container
