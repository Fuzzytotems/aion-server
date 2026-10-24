#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/CalindiSurkanaAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(CalindiSurkanaAI, "calindi_surkana");

// Java CalindiSurkanaAI.java:25-39
void CalindiSurkanaAI::handleSpawned() {
	NpcAI::handleSpawned();
	calindiId = getPosition()->getMapId() == 300520000 ? 219359 : 236274;
	switch (getNpcId()) {
		case 730695:
		case 731629:
			startReflectBuffTask(20590);
			break;
		case 730696:
		case 731630:
			startReflectBuffTask(20591);
			break;
	}
}

// Java CalindiSurkanaAI.java:41-50
void CalindiSurkanaAI::startReflectBuffTask(int32_t buffId) {
	reflectTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, buffId] {
		runtime::Ptr<Npc> calindi = getPosition()->getWorldMapInstance()->getNpc(calindiId.get());
		if (isDead() || calindi == nullptr || calindi->isDead()) {
			reflectTask.get()->cancel(false);
			return;
		}
		SkillEngine::getInstance().applyEffectDirectly(buffId, getOwner(), *getPosition()->getWorldMapInstance()->getNpc(calindiId.get()));
	}, 2500, 5000));
}

// Java CalindiSurkanaAI.java:52-57
void CalindiSurkanaAI::handleDied() {
	NpcAI::handleDied();
	if (runtime::FutureRef task = reflectTask.get(); task != nullptr && !task->isCancelled())
		task->cancel(false);
}

// Java CalindiSurkanaAI.java:59-65
bool CalindiSurkanaAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
