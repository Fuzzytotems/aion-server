#include "aion/gameserver/handlers/ai/instance/drakenspire/SealGuardianAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(SealGuardianAI, "drakenspire_seal_guardian");

// Java SealGuardianAI.java:40-43
ItemAttackType SealGuardianAI::modifyAttackType(ItemAttackType type) {
	return ItemAttackType::MAGICAL_WIND;
}

// Java SealGuardianAI.java:45-50
runtime::Ptr<Player> SealGuardianAI::getLastAttacker() {
	runtime::Ptr<AggroInfo> lastAttacker;
	for (const runtime::Ptr<AggroInfo>& ai : getAggroList().stream()) {
		runtime::Ptr<Player> player = runtime::as<Player>(ai->getAttacker());
		if (player == nullptr || player->isDead())
			continue;
		if (lastAttacker == nullptr || ai->getLastInteractionTime() > lastAttacker->getLastInteractionTime()) // max: the first of the greatest
			lastAttacker = ai;
	}
	return lastAttacker != nullptr ? runtime::as<Player>(lastAttacker->getAttacker()) : nullptr;
}

// Java SealGuardianAI.java:52-58
bool SealGuardianAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return AggressiveNoLootNpcAI::ask(question);
	}
}

// Java SealGuardianAI.java:60-64
void SealGuardianAI::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21882 && skillLevel == 57)
		// Intruder…
		PacketSendUtility::broadcastPacket(getOwner(), SM_SYSTEM_MESSAGE(ChatType::NPC, runtime::Ptr<VisibleObject>(getOwner()), 1501357, {}));
}

// Java SealGuardianAI.java:66-70
void SealGuardianAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21882) {
		// Java: addHate(null, ...) is a no-op (AggroList.isAware(null) is false)
		if (runtime::Ptr<Creature> target = getAggroList().getTarget(AggroTarget::RANDOM); target != nullptr)
			getAggroList().addHate(*target, 10000);
	}
}

// Java SealGuardianAI.java:72-78
void SealGuardianAI::handleSpawned() {
	AggressiveNoLootNpcAI::handleSpawned();
	getOwner().getGameStats()->setNextSkillDelay(0);
	startIdleTimer();
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawnSpecter(); }, 1000);
}

// Java SealGuardianAI.java:80-82
void SealGuardianAI::startIdleTimer() {
	idleTimer.set(ThreadPoolManager::getInstance().schedule({this}, [this] { despawn(); }, 60 * 1000)); // 60, TimeUnit.SECONDS
}

// Java SealGuardianAI.java:84-88
void SealGuardianAI::despawn() {
	// Teleport…
	PacketSendUtility::broadcastPacket(getOwner(), SM_SYSTEM_MESSAGE(ChatType::NPC, runtime::Ptr<VisibleObject>(getOwner()), 1501358, {}));
	notifyBeritra(2);
	AIActions::deleteOwner(*this);
}

// Java SealGuardianAI.java:90-104
void SealGuardianAI::spawnSpecter() {
	switch (getNpcId()) {
		case 855460:
			spawn(855452, 141.618f, 498.609f, 1749.590f, int8_t{30});
			break;
		case 855461:
			spawn(855454, 172.045f, 509.876f, 1749.590f, int8_t{45});
			break;
		case 855462:
			spawn(855456, 172.142f, 525.665f, 1749.590f, int8_t{75});
			break;
		case 855463:
			spawn(855458, 142.027f, 536.810f, 1749.590f, int8_t{90});
			break;
		// Will only spawn during dragon phase
		case 855464:
		case 855465:
		case 855466:
		case 855467:
		case 855468:
		case 855469:
			spawn(855452, 141.618f, 498.609f, 1749.590f, int8_t{30});
			spawn(855454, 172.045f, 509.876f, 1749.590f, int8_t{45});
			spawn(855456, 172.142f, 525.665f, 1749.590f, int8_t{75});
			spawn(855458, 142.027f, 536.810f, 1749.590f, int8_t{90});
			break;
	}
}

// Java SealGuardianAI.java:106-111
void SealGuardianAI::handleCreatureAggro(Creature& creature) {
	AggressiveNoLootNpcAI::handleCreatureAggro(creature);
	if (isIdling.compareAndSet(true, false))
		cancelTask();
}

// Java SealGuardianAI.java:113-117
void SealGuardianAI::handleBackHome() {
	AggressiveNoLootNpcAI::handleBackHome();
	despawn();
}

// Java SealGuardianAI.java:119-128
void SealGuardianAI::handleDied() {
	runtime::Ptr<Player> lastAttacker = getLastAttacker();
	if (lastAttacker != nullptr)
		SkillEngine::getInstance().applyEffectDirectly(21625, getOwner(), *lastAttacker);
	// I shall… curse you…
	PacketSendUtility::broadcastPacket(getOwner(), SM_SYSTEM_MESSAGE(ChatType::NPC, runtime::Ptr<VisibleObject>(getOwner()), 1501359, {}));

	notifyBeritra(1);
	AggressiveNoLootNpcAI::handleDied();
}

// Java SealGuardianAI.java:130-135
void SealGuardianAI::handleDespawned() {
	cancelTask();
	despawnSpecters();
	AggressiveNoLootNpcAI::handleDespawned();
}

// Java SealGuardianAI.java:137-140
void SealGuardianAI::cancelTask() {
	if (runtime::FutureRef task = idleTimer.get(); task != nullptr && !task->isDone())
		task->cancel(true);
}

// Java SealGuardianAI.java:142-152
void SealGuardianAI::despawnSpecters() {
	std::vector<runtime::Ptr<Npc>> specters; // Java: int[] specterIds = switch (getNpcId()) ..., then getNpcs(specterIds)
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	switch (getNpcId()) {
		case 855460:
			specters = instance->getNpcs({855452});
			break;
		case 855461:
			specters = instance->getNpcs({855454});
			break;
		case 855462:
			specters = instance->getNpcs({855456});
			break;
		case 855463:
			specters = instance->getNpcs({855458});
			break;
		default:
			specters = instance->getNpcs({855452, 855454, 855456, 855458});
			break;
	}

	for (const runtime::Ptr<Npc>& npc : specters)
		npc->getController().deleteIfAliveOrCancelRespawn();
}

// Java SealGuardianAI.java:154-159
void SealGuardianAI::notifyBeritra(int32_t eventId) {
	std::vector<runtime::Ptr<Npc>> possibleBeritras = getPosition()->getWorldMapInstance()->getNpcs({236244, 236245, 236246});
	if (!possibleBeritras.empty()) {
		possibleBeritras.front()->getAi().onCustomEvent(eventId); // 1 = Death, 2 = back home | 60s idle
	}
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
