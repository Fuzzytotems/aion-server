#include "aion/gameserver/handlers/ai/instance/stonespearReach/KebbitAI.h"

#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::instance::stonespearReach {

AION_AI(KebbitAI, "stonespear_kebbit");

// Java KebbitAI.java:26-30
void KebbitAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	despawnTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 15500)); // 15,5s
}

// Java KebbitAI.java:32-37
void KebbitAI::handleDied() {
	cancelTask();
	GeneralNpcAI::handleDied();
	getOwner().getController().delete_();
}

// Java KebbitAI.java:39-42
void KebbitAI::cancelTask() {
	runtime::FutureRef task = despawnTask.get();
	if (task != nullptr && !task->isDone())
		task->cancel(false);
}

// Java KebbitAI.java:44-50
bool KebbitAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::ALLOW_DECAY:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

// Java KebbitAI.java:52-55
void KebbitAI::handleAttack(runtime::Ptr<Creature> creature) {
	// do nothing
	static_cast<void>(creature);
}

} // namespace aion::gameserver::handlers::ai::instance::stonespearReach
