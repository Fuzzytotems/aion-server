#include "aion/gameserver/services/vortex/DimensionalVortex.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"

namespace aion::gameserver::services::vortex {

DimensionalVortex::DimensionalVortex(model::vortex::VortexLocation& value)
	: vortexLocation(runtime::Ref<model::vortex::VortexLocation>(value)) {
}

void DimensionalVortex::start() {
	AION_UNPORTED();
}

void DimensionalVortex::stop() {
	AION_UNPORTED();
}

// lambda at DimensionalVortex.java:80 (fieldmap key vortex.DimensionalVortex@L80:55)
void DimensionalVortex::initRiftGenerator() {
	AION_UNPORTED();
}

void DimensionalVortex::spawn(model::vortex::VortexStateType type) {
	AION_UNPORTED();
}

void DimensionalVortex::despawn() {
	AION_UNPORTED();
}

bool DimensionalVortex::isFinished() {
	AION_UNPORTED();
}

int32_t DimensionalVortex::getVortexLocationId() {
	AION_UNPORTED();
}

DimensionalVortex::~DimensionalVortex() = default;

} // namespace aion::gameserver::services::vortex
