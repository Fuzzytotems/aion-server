#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/IsbariyaTheResoluteAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(IsbariyaTheResoluteAI, "isbariya");

// Java IsbariyaTheResoluteAI.java:39-48
void IsbariyaTheResoluteAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isStart.compareAndSet(false, true)) {
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 342051, 1000);
		getPosition()->getWorldMapInstance()->setDoorState(535, false);
		startBasicSkillTask();
	}
	hpPhases->tryEnterNextPhase(*this);
}

// Java IsbariyaTheResoluteAI.java:50-59
void IsbariyaTheResoluteAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 75:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_Boss_ArchPriest_3phase());
			launchSpecial();
			break;
		case 50:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_Boss_ArchPriest_2phase());
			break;
	}
}

// Java IsbariyaTheResoluteAI.java:61-70
void IsbariyaTheResoluteAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	soulLocations.add(Point3D::create(1580.5f, 1572.8f, 304.64f));
	soulLocations.add(Point3D::create(1582.1f, 1571.2f, 304.64f));
	soulLocations.add(Point3D::create(1583.3f, 1569.9f, 304.64f));
	soulLocations.add(Point3D::create(1585.3f, 1568.1f, 304.64f));
	soulLocations.add(Point3D::create(1586.4f, 1567.1f, 304.64f));
	soulLocations.add(Point3D::create(1588.3f, 1566.2f, 304.64f));
}

// Java IsbariyaTheResoluteAI.java:72-78
void IsbariyaTheResoluteAI::handleDied() {
	cancelTasks({spawnTask.get(), basicSkillTask.get()});
	AggressiveNpcAI::handleDied();
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 342055);
	getPosition()->getWorldMapInstance()->setDoorState(535, true);
}

// Java IsbariyaTheResoluteAI.java:80-84
void IsbariyaTheResoluteAI::handleDespawned() {
	cancelTasks({spawnTask.get(), basicSkillTask.get()});
	AggressiveNpcAI::handleDespawned();
}

// Java IsbariyaTheResoluteAI.java:86-94
void IsbariyaTheResoluteAI::handleBackHome() {
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 342056);
	cancelTasks({spawnTask.get(), basicSkillTask.get()});
	AggressiveNpcAI::handleBackHome();
	isStart.set(false);
	getPosition()->getWorldMapInstance()->setDoorState(535, true);
	hpPhases->reset();
}

// Java IsbariyaTheResoluteAI.java:96-103
void IsbariyaTheResoluteAI::startBasicSkillTask() {
	basicSkillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead())
			cancelTasks({basicSkillTask.get()});
		else
			SkillEngine::getInstance()
				.getSkill(getOwner(), 18912 + commons::utils::Rnd::nextInt(2), 55, runtime::Ptr<VisibleObject>(getOwner()))
				->useNoAnimationSkill();
	}, 0, 24000));
}

// Java IsbariyaTheResoluteAI.java:105-128
void IsbariyaTheResoluteAI::launchSpecial() {
	if (isDead() || hpPhases->getCurrentPhase() == 0 || getOwner().getPosition()->getWorldMapInstance() == nullptr) {
		cancelTasks({basicSkillTask.get()});
		return;
	}
	int32_t delay = 10000;

	switch (hpPhases->getCurrentPhase()) {
		case 1:
			SkillEngine::getInstance().getSkill(getOwner(), 18959, 50, runtime::Ptr<VisibleObject>(getRandomTarget()))->useNoAnimationSkill();
			spawnSouls();
			delay = 25000;
			break;
		case 2:
			rndSpawn(281660, 5);
			break;
		case 3:
			rndSpawn(281659, 1);
			AIActions::useSkill(*this, 18993);
			delay = 20000;
			break;
	}
	scheduleSpecial(delay);
}

// Java IsbariyaTheResoluteAI.java:130-133
void IsbariyaTheResoluteAI::rndSpawn(int32_t npcId, int32_t count) {
	for (int32_t i = 0; i < count; i++)
		rndSpawnInRange(npcId, 5);
}

// Java IsbariyaTheResoluteAI.java:135-144
void IsbariyaTheResoluteAI::spawnSouls() {
	std::vector<runtime::Ptr<Point3D>> points = soulLocations.snapshot(); // Java: new ArrayList<>(soulLocations)
	int32_t count = commons::utils::Rnd::get(3, 6);
	for (int32_t i = 0; i < count; i++) {
		if (!points.empty()) {
			const size_t index = static_cast<size_t>(commons::utils::Rnd::nextInt(static_cast<int32_t>(points.size())));
			runtime::Ptr<Point3D> spawnPoint = points[index]; // Java: spawn (the name clashes with spawn())
			points.erase(points.begin() + static_cast<std::ptrdiff_t>(index));
			spawn(281645, spawnPoint->getX(), spawnPoint->getY(), spawnPoint->getZ(), int8_t{18});
		}
	}
}

// Java IsbariyaTheResoluteAI.java:146-148
runtime::Ptr<Creature> IsbariyaTheResoluteAI::getRandomTarget() {
	return getAggroList().getTarget(AggroTarget::RANDOM_EXCEPT_CURRENT_TARGET, 40);
}

// Java IsbariyaTheResoluteAI.java:150-152
void IsbariyaTheResoluteAI::scheduleSpecial(int32_t delay) {
	spawnTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { launchSpecial(); }, delay));
}

// Java IsbariyaTheResoluteAI.java:154-158
void IsbariyaTheResoluteAI::cancelTasks(std::initializer_list<runtime::FutureRef> tasks) {
	for (const runtime::FutureRef& task : tasks)
		if (task != nullptr && !task->isCancelled())
			task->cancel(true);
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
