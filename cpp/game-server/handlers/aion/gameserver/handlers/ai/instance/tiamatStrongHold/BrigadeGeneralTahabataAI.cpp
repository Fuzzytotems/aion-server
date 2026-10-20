#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralTahabataAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(BrigadeGeneralTahabataAI, "brigadegeneraltahabata");

// Java BrigadeGeneralTahabataAI.java:37-43
void BrigadeGeneralTahabataAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false))
		getPosition()->getWorldMapInstance()->setDoorState(610, false);
	hpPhases->tryEnterNextPhase(*this);
}

// Java BrigadeGeneralTahabataAI.java:45-50
void BrigadeGeneralTahabataAI::startPiercingStrikeTask() {
	piercingStrikeTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (!isDead())
			startPiercingStrikeEvent();
	}, 15000, 20000));
}

// Java BrigadeGeneralTahabataAI.java:52-57
void BrigadeGeneralTahabataAI::startFireStormTask() {
	fireStormTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (!isDead())
			startFireStormEvent();
	}, 10000, 20000));
}

// Java BrigadeGeneralTahabataAI.java:59-64
void BrigadeGeneralTahabataAI::startFireStormEvent() {
	if (getPosition()->getWorldMapInstance()->getNpc(283045) == nullptr) {
		AIActions::useSkill(*this, 20758);
		rndSpawn(283045, commons::utils::Rnd::get(1, 4));
	}
}

// Java BrigadeGeneralTahabataAI.java:66-74 (getSkill returns a skill: Java's null check stays)
void BrigadeGeneralTahabataAI::startPiercingStrikeEvent() {
	teleportRandomPlayer();
	runtime::Ref<Skill> skill = SkillEngine::getInstance().getSkill(getOwner(), 20754, 60, runtime::Ptr<VisibleObject>(getOwner()));
	if (skill != nullptr) {
		skill->useNoAnimationSkill();
		if (!skill->getEffectedList().isEmpty())
			useMultiSkill();
	}
}

// Java BrigadeGeneralTahabataAI.java:76-78
void BrigadeGeneralTahabataAI::useMultiSkill() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(*this, 20755); }, 3000);
}

// Java BrigadeGeneralTahabataAI.java:80-87
void BrigadeGeneralTahabataAI::teleportRandomPlayer() {
	runtime::Ptr<Creature> target = getAggroList().getTarget(AggroTarget::RANDOM, 40);
	if (target == nullptr)
		return;
	getOwner().setTarget(target);
	World::getInstance().updatePosition(getOwner(), target->getX(), target->getY(), target->getZ(), int8_t{0});
	PacketSendUtility::broadcastPacketAndReceive(getOwner(), SM_FORCED_MOVE(getOwner(), getOwner()));
}

// Java BrigadeGeneralTahabataAI.java:89-93
void BrigadeGeneralTahabataAI::cancelPiercingStrike() {
	runtime::FutureRef task = piercingStrikeTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java BrigadeGeneralTahabataAI.java:95-99
void BrigadeGeneralTahabataAI::cancelFireStorm() {
	runtime::FutureRef task = fireStormTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java BrigadeGeneralTahabataAI.java:101-135
void BrigadeGeneralTahabataAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 96:
			lavaEruptionEvent(283116); // 4.0
			startPiercingStrikeTask();
			break;
		case 75:
			AIActions::useSkill(*this, 20761);
			startFireStormTask();
			break;
		case 60:
			AIActions::useSkill(*this, 20761);
			break;
		case 55:
			lavaEruptionEvent(283118); // 4.0
			break;
		case 40:
		case 25:
			AIActions::useSkill(*this, 20761);
			break;
		case 20:
			cancelFireStorm();
			cancelPiercingStrike();
			lavaEruptionEvent(283120); // 4.0
			break;
		case 10:
			AIActions::useSkill(*this, 20942);
			break;
		case 7:
			AIActions::useSkill(*this, 20883);
			spawn(283102, 679.88f, 1068.88f, 497.88f, int8_t{0}); // 4.0
			break;
		default:
			break;
	}
}

// Java BrigadeGeneralTahabataAI.java:137-144
void BrigadeGeneralTahabataAI::lavaEruptionEvent(int32_t floorId) {
	AIActions::targetSelf(*this);
	AIActions::useSkill(*this, 20756);
	if (getPosition()->getWorldMapInstance()->getNpc(283051) == nullptr) { // 4.0
		spawn(283051, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}); // 4.0
	}
	spawnfloor(floorId);
}

// Java BrigadeGeneralTahabataAI.java:146-151
void BrigadeGeneralTahabataAI::spawnfloor(int32_t floor) {
	ThreadPoolManager::getInstance().schedule({this}, [this, floor] {
		spawn(floor, 679.88f, 1068.88f, 497.88f, int8_t{0});
		spawn(floor + 1, 679.88f, 1068.88f, 497.88f, int8_t{0});
	}, 10000);
}

// Java BrigadeGeneralTahabataAI.java:153-156
void BrigadeGeneralTahabataAI::rndSpawn(int32_t npcId, int32_t count) {
	for (int32_t i = 0; i < count; i++)
		rndSpawnInRange(npcId, 10);
}

// Java BrigadeGeneralTahabataAI.java:158-160
void BrigadeGeneralTahabataAI::deleteAdds() {
	for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({283116, 283117, 283118, 283119, 283120, 283121, 283102}))
		npc->getController().delete_();
}

// Java BrigadeGeneralTahabataAI.java:162-172
void BrigadeGeneralTahabataAI::handleBackHome() {
	isHome.set(true);
	getPosition()->getWorldMapInstance()->setDoorState(610, true);
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
	getEffectController()->removeEffect(20942);
	deleteAdds();
	cancelPiercingStrike();
	cancelFireStorm();
}

// Java BrigadeGeneralTahabataAI.java:174-179
void BrigadeGeneralTahabataAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelPiercingStrike();
	cancelFireStorm();
}

// Java BrigadeGeneralTahabataAI.java:181-188
void BrigadeGeneralTahabataAI::handleDied() {
	getPosition()->getWorldMapInstance()->setDoorState(610, true);
	AggressiveNpcAI::handleDied();
	deleteAdds();
	cancelPiercingStrike();
	cancelFireStorm();
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
