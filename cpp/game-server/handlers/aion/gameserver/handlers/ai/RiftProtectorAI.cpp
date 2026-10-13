#include "aion/gameserver/handlers/ai/RiftProtectorAI.h"

#include "aion/gameserver/model/stats/calc/Stat2.h"

namespace aion::gameserver::handlers::ai {

AION_AI(RiftProtectorAI, "rift_protector");

// Java RiftProtectorAI.java:19-22
void RiftProtectorAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP)
		stat.setBaseRate(0.1f);
}

} // namespace aion::gameserver::handlers::ai
