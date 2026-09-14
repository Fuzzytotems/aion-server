#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player::motion {

Motion::Motion(int32_t idValue, int32_t deletionTimeValue, bool isActive) : id(idValue), deletionTime(deletionTimeValue), active(isActive) {
}

Motion::~Motion() = default;

runtime::Ref<Motion> Motion::create(int32_t idValue, int32_t deletionTimeValue, bool isActive) {
	return runtime::makeRef<Motion>(idValue, deletionTimeValue, isActive);
}

void Motion::onExpire(Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player::motion
