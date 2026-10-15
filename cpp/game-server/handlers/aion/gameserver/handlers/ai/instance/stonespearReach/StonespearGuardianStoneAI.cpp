#include "aion/gameserver/handlers/ai/instance/stonespearReach/StonespearGuardianStoneAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::instance::stonespearReach {

AION_AI(StonespearGuardianStoneAI, "stonespear_guardian_stone");

// Java StonespearGuardianStoneAI.java:26-30
void StonespearGuardianStoneAI::handleSpawned() {
	NpcAI::handleSpawned();
	startTask();
}

// Java StonespearGuardianStoneAI.java:32-37
void StonespearGuardianStoneAI::startTask() {
	task.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_OBJ_End());
		getOwner().getController().delete_();
	}, 55000)); // message says 2mins but its actually only ~1min.
}

// Java StonespearGuardianStoneAI.java:39-45
void StonespearGuardianStoneAI::handleDied() {
	NpcAI::handleDied();
	getOwner().getController().delete_();
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isCancelled())
		current->cancel(true);
}

// Java StonespearGuardianStoneAI.java:47-52
void StonespearGuardianStoneAI::handleDespawned() {
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isCancelled())
		current->cancel(true);
	NpcAI::handleDespawned();
}

// Java StonespearGuardianStoneAI.java:54-60
bool StonespearGuardianStoneAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::ALLOW_DECAY:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::stonespearReach
