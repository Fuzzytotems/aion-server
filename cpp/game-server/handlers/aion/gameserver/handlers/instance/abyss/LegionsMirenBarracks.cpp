#include "aion/gameserver/handlers/instance/abyss/LegionsMirenBarracks.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(LegionsMirenBarracks, 301250000);

// Java LegionsMirenBarracks.java:15-17
LegionsMirenBarracks::LegionsMirenBarracks(WorldMapInstance& instance) : MirenBarracks(instance) {
}

runtime::Ref<LegionsMirenBarracks> LegionsMirenBarracks::create(WorldMapInstance& instance) {
	return runtime::makeRef<LegionsMirenBarracks>(instance);
}

// Java LegionsMirenBarracks.java:20-22
int32_t LegionsMirenBarracks::getChestId() {
	return 702298;
}

} // namespace aion::gameserver::handlers::instance::abyss
