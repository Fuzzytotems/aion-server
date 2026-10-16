#include "aion/gameserver/handlers/ai/worlds/levinshor/AncientMonsterAI.h"

#include "aion/gameserver/ai/handler/TargetEventHandler.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::worlds::levinshor {

AION_AI(AncientMonsterAI, "LDF4_Advance_Ancient_Monster");

// Java AncientMonsterAI.java:23-30
void AncientMonsterAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	getOwner().getController().addTask(TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead())
			getOwner().getController().delete_();
	}, 1000 * 60 * 60));
}

// Java AncientMonsterAI.java:32-37
void AncientMonsterAI::handleMoveArrived() {
	AggressiveNpcAI::handleMoveArrived();
	if (getOwner().getDistanceToSpawnLocation() > 15)
		TargetEventHandler::onTargetGiveup(*this);
}

// Java AncientMonsterAI.java:39-45
float AncientMonsterAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	float multi = 1.5f;
	if (effect != nullptr && effect->getSkillTemplate()->getSkillId() == 21780)
		multi = 3;
	return AggressiveNpcAI::modifyOwnerDamage(damage * multi, effected, effect);
}

} // namespace aion::gameserver::handlers::ai::worlds::levinshor
