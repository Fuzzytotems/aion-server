#include "aion/gameserver/handlers/ai/instance/padmarashkasCave/PadmarashkaAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::padmarashkasCave {

AION_AI(PadmarashkaAI, "padmarashka");

// Java PadmarashkaAI.java:37-41
void PadmarashkaAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	putToSleep();
}

// Java PadmarashkaAI.java:43-50
void PadmarashkaAI::handleAttack(runtime::Ptr<Creature> creature) {
	if (!this->getEffectController()->hasAbnormalEffect(19186) && !isStart) {
		wakeUp();
	}
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java PadmarashkaAI.java:52-58
void PadmarashkaAI::handleCreatureAggro(Creature& creature) {
	if (!this->getEffectController()->hasAbnormalEffect(19186) && !isStart) {
		wakeUp();
	}
	AggressiveNpcAI::handleCreatureAggro(creature);
}

// Java PadmarashkaAI.java:60-65
void PadmarashkaAI::wakeUp() {
	canThink_ = true;
	isStart = true;
	scheduleSpawnEntrance();
	startMainSkillTask();
}

// Java PadmarashkaAI.java:67-75 (the anonymous Runnable is a lambda)
void PadmarashkaAI::startMainSkillTask() {
	mainSkillTask = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		skillengine::SkillEngine::getInstance().getSkill(getOwner(), commons::utils::Rnd::nextInt(2) == 0 ? 20401 : 20093, 48, getTarget())->useNoAnimationSkill();
	}, 10000, 20000);
}

// Java PadmarashkaAI.java:77-84
void PadmarashkaAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 95:
			stage1();
			break;
		case 50:
			stage2();
			break;
		case 25:
			stage3();
			break;
	}
}

// Java PadmarashkaAI.java:86-99 (Java's cast of the spawn to Npc: the entrance adds are npcs)
void PadmarashkaAI::scheduleSpawnEntrance() {
	int32_t delay = 60000;
	if (isDead() || !isStart)
		return;
	else {
		getOwner().clearAttackedCount();
		int32_t count = commons::utils::Rnd::get(1, 3);
		for (int32_t i = 0; i < count; i++) {
			int32_t npcId = 282792 + commons::utils::Rnd::nextInt(5);
			attackPlayer(*runtime::cast<Npc>(spawn(npcId, 531.465f + static_cast<float>(commons::utils::Rnd::get(-5, 5)),
				316.921f + static_cast<float>(commons::utils::Rnd::get(-5, 5)), 65.351f, int8_t{105})));
		}
		scheduleDelayEntrance(delay);
	}
}

// Java PadmarashkaAI.java:101-113 (the anonymous Runnable captures the npc)
void PadmarashkaAI::attackPlayer(Npc& npc) {
	ThreadPoolManager::getInstance().schedule({this}, [this, npc = runtime::Ref<Npc>(npc)] {
		npc->setTarget(getTarget());
		npc->getAi().setStateIfNot(AIState::WALKING);
		npc->setState(CreatureState::ACTIVE, true);
		npc->getMoveController()->moveToTargetObject();
		PacketSendUtility::broadcastPacket(*npc, network::aion::serverpackets::SM_EMOTION(*npc, EmotionType::CHANGE_SPEED, 0, npc->getObjectId()));
	}, 1000);
}

// Java PadmarashkaAI.java:115-161
void PadmarashkaAI::stage1() {
	int32_t delay = 60000;
	if (isDead() || !isStart)
		return;
	else {
		skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19177, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		switch (commons::utils::Rnd::get(1, 10)) {
			case 1:
				spawn(282613, 575.064f, 163.573f, 66.01f, int8_t{0});
				break;
			case 2:
				spawn(282613, 587.361f, 165.187f, 66.01f, int8_t{0});
				break;
			case 3:
				spawn(282613, 577.463f, 155.553f, 66.01f, int8_t{0});
				break;
			case 4:
				spawn(282613, 582.064f, 160.785f, 66.01f, int8_t{0});
				break;
			case 5:
				spawn(282613, 586.160f, 157.214f, 66.01f, int8_t{0});
				break;
			case 6:
				spawn(282613, 571.436f, 150.558f, 66.01f, int8_t{0});
				break;
			case 7:
				spawn(282613, 577.574f, 150.415f, 66.01f, int8_t{0});
				break;
			case 8:
				spawn(282613, 581.354f, 151.640f, 66.01f, int8_t{0});
				break;
			case 9:
				spawn(282613, 589.382f, 149.758f, 66.01f, int8_t{0});
				break;
			case 10:
				spawn(282613, 583.469f, 145.809f, 66.01f, int8_t{0});
				break;
		}
		scheduleDelayStage1(delay);
		if (getPosition()->getWorldMapInstance()->getNpc(218675) == nullptr) {
			spawn(218675, 573.413f, 179.518f, 66.220f, int8_t{30});
		} else if (getPosition()->getWorldMapInstance()->getNpc(218676) == nullptr) {
			spawn(218676, 581.827f, 179.587f, 66.250f, int8_t{30});
		}
	}
}

// Java PadmarashkaAI.java:163-185
void PadmarashkaAI::stage2() {
	int32_t delay = 120000;
	if (isDead() || !isStart)
		return;
	else {
		PacketSendUtility::broadcastToMap(getOwner(), 1401214); // Huge egg is revealed
		switch (commons::utils::Rnd::get(1, 4)) {
			case 1:
				spawn(282614, 510.12497f, 250.17401f, 66.625f, int8_t{0});
				break;
			case 2:
				spawn(282614, 530.1312f, 170.20688f, 65.875f, int8_t{0});
				break;
			case 3:
				spawn(282614, 610.32495f, 210.19055f, 66.125f, int8_t{0});
				break;
			case 4:
				spawn(282614, 590.1816f, 290.31775f, 66.75f, int8_t{0});
				break;
		}
		scheduleDelayStage2(delay);
	}
}

// Java PadmarashkaAI.java:187-208
void PadmarashkaAI::stage3() { // Spawn Rock
	skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19179, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	PacketSendUtility::broadcastToMap(getOwner(), 1401215); // Cave crumble
	spawn(282140, 520.99585f, 270.23776f, 66.25f, int8_t{0});
	spawn(282140, 506.8137f, 238.60612f, 66.57414f, int8_t{0});
	spawn(282140, 495.94504f, 218.84671f, 67.58238f, int8_t{0});
	spawn(282140, 502.39307f, 196.61835f, 67.5513f, int8_t{0});
	spawn(282140, 510.88928f, 178.31491f, 66.869156f, int8_t{0});
	spawn(282140, 530.8308f, 160.27686f, 65.926926f, int8_t{0});
	spawn(282140, 551.6416f, 145.72856f, 67.93133f, int8_t{0});
	spawn(282140, 567.2481f, 162.51135f, 66.09399f, int8_t{0});
	spawn(282140, 585.17523f, 188.57863f, 66.125f, int8_t{0});
	spawn(282140, 611.51733f, 186.78618f, 66.25f, int8_t{0});
	spawn(282140, 624.74023f, 205.0654f, 66.255615f, int8_t{0});
	spawn(282140, 627.2288f, 228.05437f, 66.3268f, int8_t{0});
	spawn(282140, 619.18396f, 258.662f, 69.37874f, int8_t{0});
	spawn(282140, 604.1967f, 284.47342f, 66.71312f, int8_t{0});
	spawn(282140, 589.62775f, 293.01245f, 66.75f, int8_t{0});
	spawn(282140, 599.13367f, 264.5496f, 66.25f, int8_t{0});
	spawn(282140, 517.9813f, 220.19554f, 66.86278f, int8_t{0});
	spawn(282140, 533.5666f, 194.63768f, 66.125f, int8_t{0});
}

// Java PadmarashkaAI.java:210-222 (the anonymous Runnable is a lambda)
void PadmarashkaAI::scheduleDelayStage2(int32_t delay) {
	if (!isStart && !isDead())
		return;
	else {
		ThreadPoolManager::getInstance().schedule({this}, [this] { stage2(); }, delay);
	}
}

// Java PadmarashkaAI.java:224-236 (the anonymous Runnable is a lambda)
void PadmarashkaAI::scheduleDelayStage1(int32_t delay) {
	if (!isStart && !isDead())
		return;
	else {
		ThreadPoolManager::getInstance().schedule({this}, [this] { stage1(); }, delay);
	}
}

// Java PadmarashkaAI.java:238-250 (the anonymous Runnable is a lambda)
void PadmarashkaAI::scheduleDelayEntrance(int32_t delay) {
	if (!isStart && !isDead())
		return;
	else {
		ThreadPoolManager::getInstance().schedule({this}, [this] { scheduleSpawnEntrance(); }, delay);
	}
}

// Java PadmarashkaAI.java:252-256
void PadmarashkaAI::putToSleep() {
	skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19186, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	this->getEffectController()->setAbnormal(AbnormalState::SLEEP);
	canThink_ = false;
}

// Java PadmarashkaAI.java:258-263
void PadmarashkaAI::spawnPadmaProtector() {
	spawn(218670, 533.107f, 247.685f, 66.761f, int8_t{45});
	spawn(218671, 581.092f, 265.049f, 66.843f, int8_t{15});
	spawn(218673, 547.838f, 264.624f, 66.875f, int8_t{45});
	spawn(218674, 595.748f, 248.045f, 66.250f, int8_t{15});
}

// Java PadmarashkaAI.java:265-289
void PadmarashkaAI::deSpawnNpcs() {
	// Despawn Stage 3
	despawnNpcs(282140);
	// Despawn Stage 2
	despawnNpcs(282614);
	despawnNpcs(282712);
	despawnNpcs(282620);
	// Despawn Stage 1
	despawnNpcs(282613);
	despawnNpcs(218675);
	despawnNpcs(218676);
	despawnNpcs(282715);
	despawnNpcs(282716);
	// Despawn Entrance mobs
	despawnNpcs(282792);
	despawnNpcs(282793);
	despawnNpcs(282794);
	despawnNpcs(282795);
	despawnNpcs(282796);
	// Despawn Padma Protector
	despawnNpcs(218670);
	despawnNpcs(218671);
	despawnNpcs(218673);
	despawnNpcs(218674);
}

// Java PadmarashkaAI.java:291-298
void PadmarashkaAI::despawnNpcs(int32_t npcId) {
	std::vector<runtime::Ptr<Npc>> npcs = getPosition()->getWorldMapInstance()->getNpcs({npcId});
	for (const runtime::Ptr<Npc>& npc : npcs) {
		if (npc != nullptr) {
			npc->getController().delete_();
		}
	}
}

// Java PadmarashkaAI.java:300-303
void PadmarashkaAI::cancelTask() {
	runtime::FutureRef task = mainSkillTask.get();
	if (task != nullptr && !task->isDone())
		task->cancel(true);
}

// Java PadmarashkaAI.java:305-314
void PadmarashkaAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	isStart = false;
	hpPhases->reset();
	cancelTask();
	deSpawnNpcs();
	putToSleep();
	spawnPadmaProtector();
}

// Java PadmarashkaAI.java:316-322
void PadmarashkaAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
	isStart = false;
	deSpawnNpcs();
}

// Java PadmarashkaAI.java:324-329
void PadmarashkaAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTask();
	deSpawnNpcs();
}

// Java PadmarashkaAI.java:331-334
bool PadmarashkaAI::canThink() {
	return canThink_;
}

} // namespace aion::gameserver::handlers::ai::instance::padmarashkasCave
