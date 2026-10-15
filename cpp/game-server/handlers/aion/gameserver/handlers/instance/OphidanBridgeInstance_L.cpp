#include "aion/gameserver/handlers/instance/OphidanBridgeInstance_L.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(OphidanBridgeInstance_L, 301320000);

// Java OphidanBridgeInstance_L.java:12-14
OphidanBridgeInstance_L::OphidanBridgeInstance_L(WorldMapInstance& instance) : OphidanBridgeInstance(instance) {
}

runtime::Ref<OphidanBridgeInstance_L> OphidanBridgeInstance_L::create(WorldMapInstance& instance) {
	return runtime::makeRef<OphidanBridgeInstance_L>(instance);
}

} // namespace aion::gameserver::handlers::instance
