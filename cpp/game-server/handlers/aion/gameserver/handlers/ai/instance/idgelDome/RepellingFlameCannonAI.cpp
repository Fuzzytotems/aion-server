#include "aion/gameserver/handlers/ai/instance/idgelDome/RepellingFlameCannonAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::ai::instance::idgelDome {

AION_AI(RepellingFlameCannonAI, "repelling_flame_cannon");

// Java RepellingFlameCannonAI.java:25-28
float RepellingFlameCannonAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return 0;
}

// Java RepellingFlameCannonAI.java:30-34
void RepellingFlameCannonAI::handleSpawned() {
	NpcAI::handleSpawned();
	skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { AIActions::useSkill(*this, 21648); }, 1000, 1000));
}

// Java RepellingFlameCannonAI.java:36-40 (a despawn before the spawn finds skillTask null: Java's NullPointerException)
void RepellingFlameCannonAI::handleDespawned() {
	runtime::FutureRef task = skillTask.get();
	if (task == nullptr)
		throw runtime::NullPointerException("RepellingFlameCannonAI.skillTask");
	task->cancel(true);
	NpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::idgelDome
