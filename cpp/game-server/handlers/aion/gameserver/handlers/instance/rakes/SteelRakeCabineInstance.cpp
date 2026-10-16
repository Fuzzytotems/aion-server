#include "aion/gameserver/handlers/instance/rakes/SteelRakeCabineInstance.h"

#include "aion/commons/utils/Rnd.h"

namespace aion::gameserver::handlers::instance::rakes {

AION_INSTANCE_HANDLER(SteelRakeCabineInstance, 300460000);

// Java SteelRakeCabineInstance.java:14-16
SteelRakeCabineInstance::SteelRakeCabineInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<SteelRakeCabineInstance> SteelRakeCabineInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<SteelRakeCabineInstance>(instance);
}

// Java SteelRakeCabineInstance.java:18-29
void SteelRakeCabineInstance::onInstanceCreate() {
	if (commons::utils::Rnd::get(1, 2) == 1) { // Sweeper Nunukin
		spawn(219026, 353.814f, 491.557f, 949.466f, int8_t{119});
	} else {
		spawn(219026, 354.7875f, 536.17004f, 949.4662f, int8_t{7});
	}
	int32_t chance = commons::utils::Rnd::get(1, 2);
	// Madame Bovariki + Steel Rake Shaman
	spawn(chance == 1 ? 219032 : 219003, 463.124f, 512.75f, 952.545f, int8_t{0});
	spawn(chance == 1 ? 219003 : 219032, 502.859f, 548.55f, 952.417f, int8_t{85});
}

} // namespace aion::gameserver::handlers::instance::rakes
