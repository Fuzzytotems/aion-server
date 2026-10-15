#include "aion/gameserver/model/base/PanesterraFactionCamp.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/base/PanesterraBaseLocation.h"
#include "aion/gameserver/model/base/BaseOccupier.h"

namespace aion::gameserver::model::base {

// Java PanesterraFactionCamp.java:8-10
PanesterraFactionCamp::PanesterraFactionCamp(PanesterraBaseLocation& loc) : PanesterraBase(loc) {
}

runtime::Ref<PanesterraFactionCamp> PanesterraFactionCamp::create(PanesterraBaseLocation& loc) {
	return runtime::makeRef<PanesterraFactionCamp>(loc);
}

// Java PanesterraFactionCamp.java:12-15
int32_t PanesterraFactionCamp::getBossSpawnDelay() {
	return 10 * 60000;
}

// Java PanesterraFactionCamp.java:17-20
int32_t PanesterraFactionCamp::getNpcSpawnDelay() {
	return 10 * 60000; // Retail delay
}

// Java PanesterraFactionCamp.java:22-25
BaseOccupier PanesterraFactionCamp::getOccupier(runtime::Ptr<gameobjects::Creature> bossKiller) {
	static_cast<void>(bossKiller);
	return BaseOccupier::PEACE; // If the soul anchor (boss) is destroyed, the camp will be eliminated
}

PanesterraFactionCamp::~PanesterraFactionCamp() = default;

} // namespace aion::gameserver::model::base
