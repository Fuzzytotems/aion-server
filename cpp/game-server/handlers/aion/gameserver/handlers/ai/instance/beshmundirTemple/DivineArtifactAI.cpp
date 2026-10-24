#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/DivineArtifactAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(DivineArtifactAI, "divineartifact");

// Java DivineArtifactAI.java:23-30
void DivineArtifactAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (!cooldown.get()) {
		AIActions::useSkill(*this, 18915);
		setCD();
	}
}

// Java DivineArtifactAI.java:32-42
void DivineArtifactAI::setCD() { // ugly hack to prevent overflow TODO: remove on AI improve
	cooldown = true;

	ThreadPoolManager::getInstance().schedule({this}, [this] { cooldown = false; }, 1000);
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
