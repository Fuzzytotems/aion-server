#include "aion/gameserver/handlers/instance/AdmaStrongholdInstance.h"

#include "aion/commons/utils/Rnd.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(AdmaStrongholdInstance, 320130000);

// Java AdmaStrongholdInstance.java:14-16
AdmaStrongholdInstance::AdmaStrongholdInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<AdmaStrongholdInstance> AdmaStrongholdInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<AdmaStrongholdInstance>(instance);
}

// Java AdmaStrongholdInstance.java:19-27
void AdmaStrongholdInstance::onInstanceCreate() {
	if (commons::utils::Rnd::nextBoolean()) {
		switch (commons::utils::Rnd::get(1, 3)) {
			case 1:
				spawn(205224, 477.5849f, 398.20898f, 187.49918f, int8_t{50});
				break;
			case 2:
				spawn(205224, 347.76538f, 559.4453f, 180.33097f, int8_t{105});
				break;
			case 3:
				spawn(205224, 528.25116f, 555.0943f, 189.49f, int8_t{78});
				break;
			default:
				break;
		}
	}
}

} // namespace aion::gameserver::handlers::instance
