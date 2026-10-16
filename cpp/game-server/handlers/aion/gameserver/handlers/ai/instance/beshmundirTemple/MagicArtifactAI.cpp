#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/MagicArtifactAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(MagicArtifactAI, "magicartifact");

// Java MagicArtifactAI.java:23-30
void MagicArtifactAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (!cooldown.get()) {
		AIActions::useSkill(*this, 18916);
		setCD();
	}
}

// Java MagicArtifactAI.java:32-42
void MagicArtifactAI::setCD() { // ugly hack to prevent overflow TODO: remove on AI improve
	cooldown = true;

	ThreadPoolManager::getInstance().schedule({this}, [this] { cooldown = false; }, 1000);
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
