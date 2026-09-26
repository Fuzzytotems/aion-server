#include "aion/gameserver/spawnengine/WalkerGroupShift.h"

namespace aion::gameserver::spawnengine {

WalkerGroupShift::WalkerGroupShift(float leftRight, float backFront) : sagittalShift(leftRight), coronalShift(backFront) {
}

WalkerGroupShift::~WalkerGroupShift() = default;

runtime::Ref<WalkerGroupShift> WalkerGroupShift::create(float leftRight, float backFront) {
	return runtime::makeRef<WalkerGroupShift>(leftRight, backFront);
}

void WalkerGroupShift::set(WalkerGroupShift& shift) {
	sagittalShift.set(shift.sagittalShift.get());
	coronalShift.set(shift.coronalShift.get());
}

} // namespace aion::gameserver::spawnengine
