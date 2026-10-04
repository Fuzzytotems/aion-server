#include "aion/gameserver/model/summons/SummonRelease.h"

#include <utility>

#include "aion/gameserver/model/summons/UnsummonTypeInfo.h"

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
	return !started.get() && summons::isCancelableByMaster(unsummonType);
}

bool SummonRelease::cancel() {
	if (started.get())
		return false;
	runtime::Ptr<runtime::Future> pending = task.get();
	return !pending || pending->cancel(false);
}

} // namespace aion::gameserver::model::summons
