#include "aion/gameserver/handlers/instance/FireTempleInstance.h"

#include "aion/commons/utils/Rnd.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(FireTempleInstance, 320100000);

// Java FireTempleInstance.java:14-16
FireTempleInstance::FireTempleInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<FireTempleInstance> FireTempleInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<FireTempleInstance>(instance);
}

// Java FireTempleInstance.java:19-62
void FireTempleInstance::onInstanceCreate() {
	// Random spawns of bosses
	if (commons::utils::Rnd::chance() < 75) { // Blue Crystal Molgat
		spawn(212839, 127.1218f, 176.1912f, 99.67548f, int8_t{15});
	} else { // elite mob spawns
		spawn(212790, 127.1218f, 176.1912f, 99.67548f, int8_t{15});
	}
	if (commons::utils::Rnd::chance() < 75) { // Black Smoke Asparn
		spawn(212842, 322.3193f, 431.2696f, 134.5296f, int8_t{80});
	} else { // elite mob spawns
		spawn(212799, 322.3193f, 431.2696f, 134.5296f, int8_t{80});
	}
	if (commons::utils::Rnd::chance() < 75) { // Lava Gatneri
		spawn(212840, 153.0038f, 299.7786f, 123.0186f, int8_t{30});
	} else { // elite mob spawns
		spawn(212794, 153.0038f, 299.7786f, 123.0186f, int8_t{30});
	}
	if (commons::utils::Rnd::chance() < 75) { // Tough Sipus
		spawn(212843, 296.6911f, 201.9092f, 119.3652f, int8_t{30});
	} else { // elite mob spawns
		spawn(212803, 296.6911f, 201.9092f, 119.3652f, int8_t{15});
	}
	if (commons::utils::Rnd::chance() < 75) { // Flame Branch Flavi
		spawn(212841, 350.9276f, 351.7389f, 146.8498f, int8_t{45});
	} else { // elite mob spawns
		spawn(212799, 350.9276f, 351.7389f, 146.8498f, int8_t{45});
	}
	if (commons::utils::Rnd::chance() < 75) { // Broken Wing Kutisen
		spawn(212845, 298.7095f, 89.42245f, 128.7143f, int8_t{15});
	} else { // elite mob spawns
		spawn(214094, 298.7095f, 89.42245f, 128.7143f, int8_t{15});
	}
	if (commons::utils::Rnd::chance() < 10) { // stronger kromede
		spawn(214621, 421.9935f, 93.18915f, 117.3053f, int8_t{46});
	} else { // normal kromede
		spawn(212846, 421.9935f, 93.18915f, 117.3053f, int8_t{46});
	}
}

} // namespace aion::gameserver::handlers::instance
