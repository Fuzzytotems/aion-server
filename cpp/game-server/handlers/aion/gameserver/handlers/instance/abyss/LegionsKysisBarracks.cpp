#include "aion/gameserver/handlers/instance/abyss/LegionsKysisBarracks.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(LegionsKysisBarracks, 301240000);

// Java LegionsKysisBarracks.java:15-17
LegionsKysisBarracks::LegionsKysisBarracks(WorldMapInstance& instance) : KysisBarracks(instance) {
}

runtime::Ref<LegionsKysisBarracks> LegionsKysisBarracks::create(WorldMapInstance& instance) {
	return runtime::makeRef<LegionsKysisBarracks>(instance);
}

// Java LegionsKysisBarracks.java:20-22
int32_t LegionsKysisBarracks::getChestId() {
	return 702294;
}

} // namespace aion::gameserver::handlers::instance::abyss
