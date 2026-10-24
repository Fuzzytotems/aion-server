#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/AethicFieldGeneratorAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(AethicFieldGeneratorAI, "aethic_field_generator");

// Java AethicFieldGeneratorAI.java:31-34
bool AethicFieldGeneratorAI::canThink() {
	return false;
}

// Java AethicFieldGeneratorAI.java:36-40
void AethicFieldGeneratorAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	SkillEngine::getInstance().getSkill(getOwner(), 19127, 1, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
}

// Java AethicFieldGeneratorAI.java:42-50
void AethicFieldGeneratorAI::handleAttack(runtime::Ptr<Creature> creature) {
	if (isAggred.compareAndSet(false, true)) {
		getPosition()->getWorldMapInstance()->setDoorState(471, false);
		aggroTask();
	}
	GeneralNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java AethicFieldGeneratorAI.java:52-71
void AethicFieldGeneratorAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 75:
			spawn(216532, 1364.3739f, 88.38297f, 248.39449f, int8_t{37});
			spawn(216532, 1349.2109f, 87.81685f, 248.37459f, int8_t{5});
			break;
		case 50:
			spawn(216532, 1362.2529f, 88.02893f, 248.40222f, int8_t{53});
			spawn(216532, 1352.1556f, 87.69477f, 248.38733f, int8_t{4});
			spawn(216532, 1356.2352f, 90.32507f, 247.75319f, int8_t{92});
			break;
		case 25:
			spawn(216532, 1366.1492f, 84.480515f, 248.59457f, int8_t{51});
			spawn(216532, 1349.3264f, 83.13163f, 248.59457f, int8_t{6});
			spawn(216532, 1354.3655f, 85.42517f, 248.59457f, int8_t{27});
			spawn(216532, 1360.4175f, 86.17866f, 248.59457f, int8_t{34});
			break;
	}
}

// Java AethicFieldGeneratorAI.java:73-84
void AethicFieldGeneratorAI::aggroTask() {
	aggroTask_.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead()) {
			cancelAggroTask();
		} else {
			if (!isInRangePlayer()) {
				handleBackHome();
				getOwner().getController().loseAggro(true);
			}
		}
	}, 2000, 2000));
}

// Java AethicFieldGeneratorAI.java:86-88
bool AethicFieldGeneratorAI::isInRangePlayer() {
	for (const runtime::Ptr<Player>& player : getKnownList().streamVisiblePlayers())
		if (isInRange(*player, 40) && !player->isDead())
			return true;
	return false;
}

// Java AethicFieldGeneratorAI.java:90-94
void AethicFieldGeneratorAI::cancelAggroTask() {
	if (runtime::FutureRef task = aggroTask_.get(); task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

// Java AethicFieldGeneratorAI.java:96-108
void AethicFieldGeneratorAI::handleBackHome() {
	handleFinishAttack();
	cancelAggroTask();
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	if (instance != nullptr) {
		instance->setDoorState(471, true);
		deleteNpcs(instance->getNpcs({216532}));
	}
	isAggred.set(false);
	GeneralNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java AethicFieldGeneratorAI.java:110-120
void AethicFieldGeneratorAI::handleDied() {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	if (instance != nullptr) {
		instance->setDoorState(471, true);
		deleteNpcs(instance->getNpcs({216532}));
	}
	cancelAggroTask();
	GeneralNpcAI::handleDied();
}

// Java AethicFieldGeneratorAI.java:122-128
void AethicFieldGeneratorAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs) {
		if (npc != nullptr) {
			npc->getController().delete_();
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
