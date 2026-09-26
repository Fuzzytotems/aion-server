#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"

#include <unordered_map>

#include "aion/gameserver/dao/MotionDAO.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects::player::motion {

namespace {

using MotionMap = runtime::RcLinkedHashMap<int32_t, runtime::Ref<Motion>>;

/** Java Motion.motionType.get(motionId) used as a map key (Java would use a null key for an unknown id; std::map::at throws instead) */
int32_t motionTypeOf(int32_t motionId) {
	return Motion::motionType.at(motionId);
}

/** Java ExpireTimerTask.getInstance().registerExpirable(expirable, player); the task manager (P5-14) has no C++ header yet */
void registerExpirable(Motion& expirable, Player& player) {
	static_cast<void>(expirable);
	static_cast<void>(player);
	AION_UNPORTED();
}

} // namespace

MotionList::MotionList(Player& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

MotionList::~MotionList() = default;

void MotionList::add(Motion& motion, bool persist) {
	if (!motions)
		motions.set(MotionMap::create(AION_LOCK_CLASS(MotionList::motions)));
	if (motions->containsKey(motion.getId()) && motion.getExpireTime() == 0)
		remove(motion.getId());
	motions->put(motion.getId(), runtime::Ref<Motion>(motion));
	if (motion.isActive()) {
		if (!activeMotions)
			activeMotions.set(MotionMap::create(AION_LOCK_CLASS(MotionList::activeMotions)));
		runtime::Ptr<Motion> old = activeMotions->put(motionTypeOf(motion.getId()), runtime::Ref<Motion>(motion));
		if (old) {
			old->setActive(false);
			dao::MotionDAO::updateMotion(owner.getObjectId(), *old);
		}
	}
	if (persist) {
		registerExpirable(motion, owner);
		dao::MotionDAO::storeMotion(owner.getObjectId(), motion);
	}
}

bool MotionList::remove(int32_t motionId) {
	runtime::Ptr<Motion> motion = motions->remove(motionId);
	if (motion) {
		utils::PacketSendUtility::sendPacket(owner, network::aion::serverpackets::SM_MOTION(static_cast<int16_t>(motionId)));
		dao::MotionDAO::deleteMotion(owner.getObjectId(), motionId);
		if (motion->isActive()) {
			activeMotions->remove(motionTypeOf(motionId));
			return true;
		}
	}
	return false;
}

void MotionList::setActive(int32_t motionId, int32_t motionType) {
	if (motionId != 0) {
		runtime::Ptr<Motion> motion = motions->get(motionId);
		if (!motion || motion->isActive())
			return;
		if (!activeMotions)
			activeMotions.set(MotionMap::create(AION_LOCK_CLASS(MotionList::activeMotions)));
		runtime::Ptr<Motion> old = activeMotions->put(motionType, runtime::Ref<Motion>(motion));
		if (old) {
			old->setActive(false);
			dao::MotionDAO::updateMotion(owner.getObjectId(), *old);
		}
		motion->setActive(true);
		dao::MotionDAO::updateMotion(owner.getObjectId(), *motion);
	} else if (runtime::Ptr<MotionMap> active = activeMotions.get()) {
		runtime::Ptr<Motion> old = active->remove(motionType);
		if (!old)
			return; // TODO packet hack??
		old->setActive(false);
		dao::MotionDAO::updateMotion(owner.getObjectId(), *old);
	}
	utils::PacketSendUtility::sendPacket(owner,
		network::aion::serverpackets::SM_MOTION(static_cast<int16_t>(motionId), static_cast<int8_t>(motionType)));
	runtime::Ptr<MotionMap> active = activeMotions.get();
	if (!active) {
		// Deviation: Java broadcasts SM_MOTION(ownerId, null), whose writeImpl throws a NullPointerException on every recipient's write thread,
		// so nobody receives it; the C++ port skips the broadcast (docs/deviations/P4-12.md)
		return;
	}
	std::unordered_map<int32_t, runtime::Ptr<Motion>> activeSnapshot;
	for (const auto& entry : active->snapshot())
		activeSnapshot.emplace(entry.key, entry.value);
	utils::PacketSendUtility::broadcastPacket(owner, network::aion::serverpackets::SM_MOTION(owner.getObjectId(), activeSnapshot), true);
}

} // namespace aion::gameserver::model::gameobjects::player::motion
