#include "aion/gameserver/handlers/ai/instance/rentusBase/KuharaBombAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(KuharaBombAI, "kuhara_bomb");

// Java KuharaBombAI.java:25-30
void KuharaBombAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	setStateIfNot(AIState::FOLLOWING);
	boss = getPosition()->getWorldMapInstance()->getNpc(217311);
}

// Java KuharaBombAI.java:32-39
void KuharaBombAI::handleMoveArrived() {
	if (isDestroyed.compareAndSet(false, true)) {
		runtime::Ptr<Npc> kuhara = boss.get();
		if (kuhara != nullptr && !kuhara->isDead()) {
			SkillEngine::getInstance().getSkill(getOwner(), 19659, 60, kuhara)->useNoAnimationSkill();
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
