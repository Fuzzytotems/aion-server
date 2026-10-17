#include "aion/gameserver/handlers/ai/instance/drakenspire/BeritraMinionAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(BeritraMinionAI, "drakenspire_beritra_minion");

// Java BeritraMinionAI.java:30-40
void BeritraMinionAI::handleSpawned() {
	AggressiveNoLootNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { aggroPlayer(); }, 500);
	deathTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		int32_t npcId = getSkillNpcId();
		if (npcId != 0)
			spawn(npcId, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), int8_t{0});
		ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::die(*this); }, 500);
	}, 20000));
}

// Java BeritraMinionAI.java:42-45
void BeritraMinionAI::aggroPlayer() {
	for (const runtime::Ptr<Player>& p : getKnownList().streamPlayers()) {
		if (!p->isDead() && PositionUtil::isInRange(*p, 152.38f, 518.68f, 1749.6f, 24)) { // findAny
			getAggroList().addHate(*p, 10000);
			break;
		}
	}
}

// Java BeritraMinionAI.java:47-54
int32_t BeritraMinionAI::getSkillNpcId() {
	switch (getNpcId()) {
		case 855444:
			return 855447;
		case 855445:
			return 855448;
		case 855446:
			return 855449;
		default:
			return 0;
	}
}

// Java BeritraMinionAI.java:56-58
bool BeritraMinionAI::isTargetInsideArena() {
	return PositionUtil::isInRange(*getTarget(), 151.9f, 518.6f, 1749.6f, 26);
}

// Java BeritraMinionAI.java:60-77
void BeritraMinionAI::handleTargetTooFar() {
	if (!isTargetInsideArena()) {
		if (followAttempts.incrementAndGet() < 3) {
			ThreadPoolManager::getInstance().schedule({this}, [this] { handleTargetTooFar(); }, 2000);
		} else {
			followAttempts.set(0);
			getAggroList().stopHating(*getTarget());
			runtime::Ptr<Creature> mostHated = getAggroList().getTarget(AggroTarget::MOST_HATED);
			if (mostHated != nullptr)
				onCreatureEvent(gameserver::ai::event::AIEventType::TARGET_CHANGED, *mostHated);
			else
				onGeneralEvent(gameserver::ai::event::AIEventType::TARGET_GIVEUP);
		}
		return;
	}
	AggressiveNoLootNpcAI::handleTargetTooFar();
}

// Java BeritraMinionAI.java:79-84
void BeritraMinionAI::handleDespawned() {
	if (runtime::FutureRef task = deathTask.get(); task != nullptr && !task->isDone())
		task->cancel(true);
	AggressiveNoLootNpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
