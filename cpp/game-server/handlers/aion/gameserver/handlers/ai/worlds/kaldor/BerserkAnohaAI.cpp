#include "aion/gameserver/handlers/ai/worlds/kaldor/BerserkAnohaAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::worlds::kaldor {

AION_AI(BerserkAnohaAI, "berserk_anoha");

// Java BerserkAnohaAI.java:35-41
void BerserkAnohaAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	PacketSendUtility::broadcastToWorld(SM_SYSTEM_MESSAGE::STR_MSG_ANOHA_SPAWN());
	scheduleDespawn();
	occupier = SiegeService::getInstance().getFortress(7011)->getRace();
}

// Java BerserkAnohaAI.java:43-46
void BerserkAnohaAI::scheduleDespawn() {
	getOwner().getController().addTask(TaskId::DESPAWN,
		ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().deleteIfAliveOrCancelRespawn(); }, 1,
			runtime::TimeUnit::HOURS));
}

// Java BerserkAnohaAI.java:48-54
void BerserkAnohaAI::handleDespawned() {
	if (!isDead())
		PacketSendUtility::broadcastToWorld(SM_SYSTEM_MESSAGE::STR_MSG_ANOHA_DESPAWN());
	despawnFlag();
	AggressiveNpcAI::handleDespawned();
}

// Java BerserkAnohaAI.java:56-63
void BerserkAnohaAI::handleDied() {
	getOwner().getController().cancelTask(TaskId::DESPAWN);
	PacketSendUtility::broadcastToWorld(SM_SYSTEM_MESSAGE::STR_MSG_ANOHA_DIE());
	despawnFlag();
	checkForFactionReward();
	AggressiveNpcAI::handleDied();
}

// Java BerserkAnohaAI.java:65-69
void BerserkAnohaAI::despawnFlag() {
	runtime::Ptr<Npc> flag = getOwner().getPosition()->getWorldMapInstance()->getNpc(702618); // see AnohasSword AI
	if (flag != nullptr)
		flag->getController().delete_();
}

// Java BerserkAnohaAI.java:71-74
void BerserkAnohaAI::checkForFactionReward() {
	runtime::Ptr<Npc> ca = runtime::cast<Npc>(
		spawn(occupier.get() == SiegeRace::ASMODIANS ? 804594 : 804595, 785.4833f, 458.4128f, 143.7177f, int8_t{30})); // Commander Anoha
	Npc& caNpc = *ca;
	runtime::Ref<Npc> pinnedCa(caNpc); // Java: the lambda's capture of ca
	caNpc.getController().addTask(TaskId::DESPAWN,
		ThreadPoolManager::getInstance().schedule({&caNpc}, [pinnedCa] { pinnedCa->getController().delete_(); }, 60, runtime::TimeUnit::MINUTES));
}

// Java BerserkAnohaAI.java:76-86
void BerserkAnohaAI::handleCreatureSee(Creature& creature) {
	AggressiveNpcAI::handleCreatureSee(creature);
	if (runtime::Ptr<Player> player = runtime::as<Player>(creature)) { // Java: instanceof Player player
		if (occupier.get() == SiegeRace::ASMODIANS) {
			startQuest(*player, creature.getRace() == Race::ELYOS ? 13818 : 23817);
		} else if (occupier.get() == SiegeRace::ELYOS) {
			startQuest(*player, creature.getRace() == Race::ELYOS ? 13817 : 23818);
		}
	}
}

// Java BerserkAnohaAI.java:88-94
bool BerserkAnohaAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_LOOT:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java BerserkAnohaAI.java:96-101
void BerserkAnohaAI::startQuest(Player& player, int32_t questId) {
	const runtime::Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(questId);
	runtime::Ref<QuestEnv> env = QuestEnv::create(nullptr, player, questId);
	if (qs == nullptr || qs->isStartable())
		QuestService::startQuest(*env);
}

} // namespace aion::gameserver::handlers::ai::worlds::kaldor
