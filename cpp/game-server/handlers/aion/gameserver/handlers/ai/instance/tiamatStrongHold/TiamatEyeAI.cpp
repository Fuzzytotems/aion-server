#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/TiamatEyeAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(TiamatEyeAI, "tiamateye");

// Java TiamatEyeAI.java:20-39. Correction of the Java code (owner's decision 2026-10-19, both branches; was J-I6-6): the switch reads the
// ids TiamatStrongHoldInstance spawns the eyes as, 283177-283180 ("ex 283913" ...), where Java read the 4.0 ids 283913-283916
void TiamatEyeAI::handleSpawned() {
	NpcAI::handleSpawned();
	int32_t ownerId = getOwner().getNpcId(); // Java: int owner (renamed: it hides AbstractAI::owner)
	switch (ownerId) {
		case 283177:
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500679, 2000);
			break;
		case 283178:
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500680, 2000);
			break;
		case 283179:
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500681, 2000);
			break;
		case 283180:
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500682, 2000);
			break;
		default:
			break;
	}
	despawn();
}

// Java TiamatEyeAI.java:41-51 (the anonymous Runnable: a task pinning this)
void TiamatEyeAI::despawn() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			AIActions::deleteOwner(*this);
		}
	}, 5000);
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
