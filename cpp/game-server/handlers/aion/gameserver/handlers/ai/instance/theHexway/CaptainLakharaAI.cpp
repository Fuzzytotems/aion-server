#include "aion/gameserver/handlers/ai/instance/theHexway/CaptainLakharaAI.h"

#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/ai/Percentage.h"
#include "aion/gameserver/model/templates/ai/SummonGroup.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::theHexway {

AION_AI(CaptainLakharaAI, "captain_lakhara");

// Java CaptainLakharaAI.java:26-33
void CaptainLakharaAI::handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent) {
	getOwner().clearQueuedSkills();
	getOwner().queueSkill(17497, 65, 0);
	cancelDespawnTask();
	for (const model::templates::ai::SummonGroup& group : percent.getSummons())
		summonNpcWithSmoke(group);
	std::vector<int32_t> npcIds; // Java: the lambda reads percent.getSummons() (static data): their npc ids
	for (const model::templates::ai::SummonGroup& group : percent.getSummons())
		npcIds.push_back(group.getNpcId());
	despawnTask.set(ThreadPoolManager::getInstance().schedule({this}, [this, npcIds] {
		for (int32_t npcId : npcIds)
			despawnNpc(npcId);
	}, 25000));
}

// Java CaptainLakharaAI.java:35-39
void CaptainLakharaAI::handleDespawned() {
	SummonerAI::handleDespawned();
	cancelDespawnTask();
}

// Java CaptainLakharaAI.java:41-45
void CaptainLakharaAI::handleBackHome() {
	SummonerAI::handleBackHome();
	cancelDespawnTask();
}

// Java CaptainLakharaAI.java:47-50
void CaptainLakharaAI::handleDied() {
	SummonerAI::handleDied();
	cancelDespawnTask();
}

// Java CaptainLakharaAI.java:52-55
void CaptainLakharaAI::cancelDespawnTask() {
	runtime::FutureRef task = despawnTask.get();
	if (task != nullptr && !task->isDone())
		task->cancel(true);
}

// Java CaptainLakharaAI.java:57-59
void CaptainLakharaAI::despawnNpc(int32_t npcId) {
	for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({npcId}))
		npc->getController().deleteIfAliveOrCancelRespawn();
}

// Java CaptainLakharaAI.java:61-65 (Java's cast of the smoke to Npc: 282465 is an npc)
void CaptainLakharaAI::summonNpcWithSmoke(const model::templates::ai::SummonGroup& summon) {
	spawnHelpers(summon);
	runtime::Ptr<Npc> smoke = runtime::cast<Npc>(spawn(282465, summon.getX(), summon.getY(), summon.getZ(), summon.getH()));
	smoke->getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::theHexway
