#include "aion/gameserver/handlers/ai/instance/drakenspire/OrissanAI.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(OrissanAI, "orissan");

// Java OrissanAI.java:32-37
void OrissanAI::handleSpawned() {
	AggressiveNoLootNpcAI::handleSpawned();
	lastSpawnEvent = commons::utils::Rnd::get(1, 2);
	// task = ThreadPoolManager.getInstance().schedule(this::startSummoningEvent, 30000);
}

// Java OrissanAI.java:39-44
void OrissanAI::handleCreatureAggro(Creature& creature) {
	AggressiveNoLootNpcAI::handleCreatureAggro(creature);
	// if (isActive.compareAndSet(false, true))
	// task = ThreadPoolManager.getInstance().schedule(this::startSummoningEvent, 30000);
}

// Java OrissanAI.java:46-49
void OrissanAI::startSummoningEvent() {
	getOwner().clearQueuedSkills();
	getOwner().queueSkill(21646, 1, 10000);
}

// Java OrissanAI.java:51-78
void OrissanAI::onEndUseSkill(const SkillTemplate* st, int32_t skillLevel) {
	switch (st->getSkillId()) {
		case 21533: // Ice Explosion
			lastSpawnEvent = 1;
			task.set(ThreadPoolManager::getInstance().schedule({this}, [this] { startSummoningEvent(); }, 30000));
			break;
		case 21637: // Rigid Gait
			ThreadPoolManager::getInstance().schedule({this}, [this] { attackIcingCrystal(); }, 4500);
			break;
		case 21646: // Summon Freezing Crystal
			if (lastSpawnEvent.get() == 1) {
				spawnIcingCrystals();
				task.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
					getOwner().getEffectController()->setAbnormal(AbnormalState::SANCTUARY);
					attackIcingCrystal();
				}, 10000));
			} else {
				spawnFrigidCrystals();
				task.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
					getOwner().clearQueuedSkills();
					getOwner().queueSkill(21632, 1, 0); // Frigid Blast
					getOwner().queueSkill(21633, 1, 8000); // Ice Explosion
				}, 10000));
			}
			break;
	}
}

// Java OrissanAI.java:80-90
void OrissanAI::attackIcingCrystal() {
	runtime::Ptr<VisibleObject> servant = getKnownList().findObject([](world::knownlist::KnownObject& o) {
		runtime::Ptr<Npc> npc = runtime::as<Npc>(o.get());
		return npc != nullptr && npc->getNpcId() == 855700 && !npc->isDead() && !npc->getLifeStats()->isAboutToDie();
	});
	if (servant != nullptr) {
		getOwner().setTarget(servant);
		SkillEngine::getInstance().getSkill(getOwner(), 21637, 67, servant)->useSkill();
	} else {
		getOwner().getEffectController()->unsetAbnormal(AbnormalState::SANCTUARY);
		lastSpawnEvent = 2;
		task.set(ThreadPoolManager::getInstance().schedule({this}, [this] { startSummoningEvent(); }, 30000));
	}
}

// Java OrissanAI.java:92-97
void OrissanAI::spawnIcingCrystals() {
	spawn(855608, 822.0f, 567.7f, 1701.045f, int8_t{0});
	spawn(855608, 811.6f, 577.7f, 1701.045f, int8_t{0});
	spawn(855608, 811.7f, 556.5f, 1701.045f, int8_t{0});
	spawn(855608, 801.1f, 567.9f, 1701.045f, int8_t{0});
}

// Java OrissanAI.java:99-117
void OrissanAI::spawnFrigidCrystals() {
	spawn(855607, 790.7f, 567.7f, 1701.045f, int8_t{0});
	spawn(855607, 797.0f, 552.0f, 1701.045f, int8_t{0});
	spawn(855607, 797.1f, 583.1f, 1701.045f, int8_t{0});
	spawn(855607, 801.1f, 567.9f, 1701.045f, int8_t{0});
	spawn(855607, 802.5f, 557.9f, 1701.045f, int8_t{0});
	spawn(855607, 802.7f, 576.6f, 1701.045f, int8_t{0});
	spawn(855607, 811.6f, 577.8f, 1701.045f, int8_t{0});
	spawn(855607, 811.6f, 587.7f, 1701.045f, int8_t{0});
	spawn(855607, 811.7f, 556.5f, 1701.045f, int8_t{0});
	spawn(855607, 811.8f, 567.9f, 1701.045f, int8_t{0});
	spawn(855607, 812.0f, 545.5f, 1701.045f, int8_t{0});
	spawn(855607, 820.3f, 557.9f, 1701.045f, int8_t{0});
	spawn(855607, 820.5f, 576.4f, 1701.045f, int8_t{0});
	spawn(855607, 822.0f, 567.7f, 1701.045f, int8_t{0});
	spawn(855607, 826.1f, 551.2f, 1701.045f, int8_t{0});
	spawn(855607, 825.9f, 582.1f, 1701.045f, int8_t{0});
	spawn(855607, 832.8f, 567.6f, 1701.045f, int8_t{0});
}

// Java OrissanAI.java:119-123
void OrissanAI::cancelTask() {
	if (runtime::FutureRef t = task.get(); t != nullptr && !t->isDone()) {
		t->cancel(true);
	}
}

// Java OrissanAI.java:125-130
void OrissanAI::handleBackHome() {
	isActive.set(false);
	cancelTask();
	AggressiveNoLootNpcAI::handleBackHome();
}

// Java OrissanAI.java:132-136. Correction of the Java code (owner's decision 2026-10-19, both branches; was J-P6I12-1): super.handleDied();
// Java called super.handleDespawned(), so Orissan's death skipped NpcAI's death handling
void OrissanAI::handleDied() {
	cancelTask();
	AggressiveNoLootNpcAI::handleDied();
}

// Java OrissanAI.java:138-142
void OrissanAI::handleDespawned() {
	cancelTask();
	AggressiveNoLootNpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
