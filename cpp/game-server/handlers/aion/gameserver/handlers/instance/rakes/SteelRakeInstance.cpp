#include "aion/gameserver/handlers/instance/rakes/SteelRakeInstance.h"

#include "aion/commons/utils/Rnd.h"

namespace aion::gameserver::handlers::instance::rakes {

AION_INSTANCE_HANDLER(SteelRakeInstance, 300100000);

// Java SteelRakeInstance.java:14-16
SteelRakeInstance::SteelRakeInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<SteelRakeInstance> SteelRakeInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<SteelRakeInstance>(instance);
}

// Java SteelRakeInstance.java:18-27
void SteelRakeInstance::onInstanceCreate() {
	if (commons::utils::Rnd::chance() < 75) { // Pegureronerk
		spawn(798376, 516.198364f, 489.708008f, 885.760315f, int8_t{60});
	} else { // Kaneron Agent
		spawn(215041, 516.198f, 489.708f, 885.76f, int8_t{60});
	}
	int32_t specialDelivery = commons::utils::Rnd::get({215074, 215075, 215076, 215077, 215054, 215055});
	spawn(specialDelivery, 461.933350f, 510.545654f, 877.618103f, int8_t{90});
}

} // namespace aion::gameserver::handlers::instance::rakes
