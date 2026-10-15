#include "aion/gameserver/handlers/ai/instance/rentusBase/DancingFlameAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(DancingFlameAI, "dancing_flame");

// Java DancingFlameAI.java:27-30
float DancingFlameAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return 0;
}

// Java DancingFlameAI.java:32-37
void DancingFlameAI::startBuffTask() {
	buffTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		bool anyInRange = false; // Java: getKnownList().streamPlayers().anyMatch(p -> isInRange(p, 10))
		for (const runtime::Ptr<Player>& p : getKnownList().streamPlayers()) {
			if (isInRange(*p, 10)) {
				anyInRange = true;
				break;
			}
		}
		if (anyInRange)
			SkillEngine::getInstance().getSkill(getOwner(), getNpcId() == 282998 ? 20536 : 20535, 60, runtime::Ptr<VisibleObject>(getOwner()))
				->useNoAnimationSkill();
	}, 10000, 9000));
}

// Java DancingFlameAI.java:39-43
void DancingFlameAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	startBuffTask();
}

// Java DancingFlameAI.java:45-49
void DancingFlameAI::handleDespawned() {
	buffTask.get()->cancel(true);
	GeneralNpcAI::handleDespawned();
}

// Java DancingFlameAI.java:51-57
bool DancingFlameAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::ALLOW_DECAY:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
