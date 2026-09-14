#include "aion/gameserver/spawnengine/WalkerGroupShift.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::spawnengine {

WalkerGroupShift::WalkerGroupShift(float leftRight, float backFront) : sagittalShift(leftRight), coronalShift(backFront) {
}

WalkerGroupShift::~WalkerGroupShift() = default;

runtime::Ref<WalkerGroupShift> WalkerGroupShift::create(float leftRight, float backFront) {
	return runtime::makeRef<WalkerGroupShift>(leftRight, backFront);
}

void WalkerGroupShift::set(WalkerGroupShift& shift) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::spawnengine
