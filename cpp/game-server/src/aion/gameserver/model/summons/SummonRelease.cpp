#include "aion/gameserver/model/summons/SummonRelease.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::summons {

SummonRelease::SummonRelease(UnsummonType unsummonTypeValue) : unsummonType(unsummonTypeValue) {
}

SummonRelease::~SummonRelease() = default;

runtime::Ref<SummonRelease> SummonRelease::create(UnsummonType unsummonTypeValue) {
	return runtime::makeRef<SummonRelease>(unsummonTypeValue);
}

void SummonRelease::setTask(runtime::FutureRef value) {
	task.set(std::move(value));
}

bool SummonRelease::isCancelableByMaster() {
	AION_UNPORTED();
}

bool SummonRelease::cancel() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::summons
