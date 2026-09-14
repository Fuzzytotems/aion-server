#include "aion/gameserver/model/stats/container/SummonLifeStats.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/SummonGameStats.h"

namespace aion::gameserver::model::stats::container {

SummonLifeStats::SummonLifeStats(gameobjects::Summon& ownerValue)
	: CreatureLifeStats(ownerValue, ownerValue.getGameStats()->getMaxHp()->getCurrent(), ownerValue.getGameStats()->getMaxMp()->getCurrent()) {
}

SummonLifeStats::~SummonLifeStats() = default;

void SummonLifeStats::triggerRestoreTask() {
	AION_UNPORTED();
}

gameobjects::Summon& SummonLifeStats::getOwner() const {
	return static_cast<gameobjects::Summon&>(CreatureLifeStats::getOwner());
}

} // namespace aion::gameserver::model::stats::container
