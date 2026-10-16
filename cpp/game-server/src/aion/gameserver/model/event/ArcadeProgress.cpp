#include "aion/gameserver/model/event/ArcadeProgress.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::event {

ArcadeProgress::ArcadeProgress(int32_t value)
	: playerObjId(value) {
}

runtime::Ref<ArcadeProgress> ArcadeProgress::create(int32_t value) {
	return runtime::makeRef<ArcadeProgress>(value);
}

ArcadeProgress::~ArcadeProgress() = default;

} // namespace aion::gameserver::model::event
