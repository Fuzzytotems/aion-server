#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/VirhanaTheGreatAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(VirhanaTheGreatAI, "virhana");

// Java VirhanaTheGreatAI.java:24-31
void VirhanaTheGreatAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (!isStart.get()) {
		isStart = true;
		scheduleRage();
	}
}

// Java VirhanaTheGreatAI.java:33-37
void VirhanaTheGreatAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	isStart = false;
}

// Java VirhanaTheGreatAI.java:39-53
void VirhanaTheGreatAI::scheduleRage() {
	if (isDead() || !isStart.get()) {
		return;
	}
	AIActions::useSkill(*this, 19121);

	ThreadPoolManager::getInstance().schedule({this}, [this] { startRage(); }, 70000);
}

// Java VirhanaTheGreatAI.java:55-75
void VirhanaTheGreatAI::startRage() {
	if (isDead() || !isStart.get()) {
		return;
	}
	if (count.get() < 12) {
		AIActions::useSkill(*this, 18897);
		count = count.get() + 1;

		ThreadPoolManager::getInstance().schedule({this}, [this] { startRage(); }, 10000);
	} else { // restart after a douzen casts
		count = 0;
		scheduleRage();
	}
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
