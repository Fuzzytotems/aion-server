#include "aion/gameserver/handlers/instance/DanuarReliquaryInstance_L.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(DanuarReliquaryInstance_L, 301330000);

// Java DanuarReliquaryInstance_L.java:12-14
DanuarReliquaryInstance_L::DanuarReliquaryInstance_L(WorldMapInstance& instance) : DanuarReliquaryInstance(instance) {
}

runtime::Ref<DanuarReliquaryInstance_L> DanuarReliquaryInstance_L::create(WorldMapInstance& instance) {
	return runtime::makeRef<DanuarReliquaryInstance_L>(instance);
}

// Java DanuarReliquaryInstance_L.java:16-19
int32_t DanuarReliquaryInstance_L::getExitId() {
	return 730843;
}

// Java DanuarReliquaryInstance_L.java:21-24
int32_t DanuarReliquaryInstance_L::getTreasureBoxId() {
	return 802183;
}

} // namespace aion::gameserver::handlers::instance
