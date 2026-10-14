#include "aion/gameserver/services/vortex/DimensionalVortex.h"

#include <string>

#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/VortexService.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"

namespace aion::gameserver::services::vortex {

DimensionalVortex::DimensionalVortex(model::vortex::VortexLocation& value)
	: vortexLocation(runtime::Ref<model::vortex::VortexLocation>(value)) {
}

// Java DimensionalVortex.java:43-60
void DimensionalVortex::start() {
	bool doubleStart = false;

	SYNCHRONIZED(*this) {
		if (started.get()) {
			doubleStart = true;
		} else {
			started.set(true);
		}
	}

	if (doubleStart) {
		return;
	}

	startInvasion();
}

// Java DimensionalVortex.java:62-66
void DimensionalVortex::stop() {
	if (finished.compareAndSet(false, true)) {
		stopInvasion();
	}
}

// Java DimensionalVortex.java:68-81; lambda at DimensionalVortex.java:80 (fieldmap key vortex.DimensionalVortex@L80:55): the generator's
// DeathObserver pins this vortex
void DimensionalVortex::initRiftGenerator() {
	runtime::Ptr<model::gameobjects::Npc> gen;
	for (const runtime::Ptr<model::gameobjects::VisibleObject>& obj : getVortexLocation()->getSpawned().snapshot()) {
		runtime::Ptr<model::gameobjects::Npc> npc = runtime::cast<model::gameobjects::Npc>(obj); // Java: (Npc) obj
		const int32_t npcId = npc->getNpcId();
		if (npcId == 209487 || npcId == 209486) {
			gen = npc;
		}
	}

	if (!gen) {
		throw runtime::NullPointerException("No generator was found in loc:" + std::to_string(getVortexLocationId()));
	}
	gen->getObserveController()->attach(*controllers::observer::DeathObserver::create(
		runtime::PinnedCallback<void(model::gameobjects::Creature&)>({this}, [this](model::gameobjects::Creature&) {
			VortexService::getInstance().stopInvasion(getVortexLocationId());
		})));
}

// Java DimensionalVortex.java:83-85
void DimensionalVortex::spawn(model::vortex::VortexStateType type) {
	VortexService::getInstance().spawn(*getVortexLocation(), type);
}

// Java DimensionalVortex.java:87-89
void DimensionalVortex::despawn() {
	VortexService::getInstance().despawn(*getVortexLocation());
}

// Java DimensionalVortex.java:91-93
bool DimensionalVortex::isFinished() {
	return finished.get();
}

// Java DimensionalVortex.java:99-101
int32_t DimensionalVortex::getVortexLocationId() {
	return vortexLocation->getId();
}

DimensionalVortex::~DimensionalVortex() = default;

} // namespace aion::gameserver::services::vortex
