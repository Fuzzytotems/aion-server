#include "aion/gameserver/handlers/instance/abyss/LegionsKrotanBarracks.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(LegionsKrotanBarracks, 301260000);

// Java LegionsKrotanBarracks.java:15-17
LegionsKrotanBarracks::LegionsKrotanBarracks(WorldMapInstance& instance) : KrotanBarracks(instance) {
}

runtime::Ref<LegionsKrotanBarracks> LegionsKrotanBarracks::create(WorldMapInstance& instance) {
	return runtime::makeRef<LegionsKrotanBarracks>(instance);
}

// Java LegionsKrotanBarracks.java:20-22
int32_t LegionsKrotanBarracks::getChestId() {
	return 702290;
}

} // namespace aion::gameserver::handlers::instance::abyss
