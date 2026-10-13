#include "aion/gameserver/handlers/ai/siege/ArtifactProtectorAI.h"

#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(ArtifactProtectorAI, "artifact_protector");

// Java ArtifactProtectorAI.java:19-23
void ArtifactProtectorAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP && getOwner().getLevel() >= 65)
		stat.setBaseRate(SiegeConfig::ARTIFACT_PROTECTOR_HEALTH_MULTIPLIER.load());
}

} // namespace aion::gameserver::handlers::ai::siege
