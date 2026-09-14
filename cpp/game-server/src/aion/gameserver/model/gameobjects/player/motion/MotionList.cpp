#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"

namespace aion::gameserver::model::gameobjects::player::motion {

MotionList::MotionList(Player& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

MotionList::~MotionList() = default;

void MotionList::add(Motion& motion, bool persist) {
	AION_UNPORTED();
}

bool MotionList::remove(int32_t motionId) {
	AION_UNPORTED();
}

void MotionList::setActive(int32_t motionId, int32_t motionType) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player::motion
