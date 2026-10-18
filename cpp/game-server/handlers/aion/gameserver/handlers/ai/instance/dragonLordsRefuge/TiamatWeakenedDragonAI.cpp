#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatWeakenedDragonAI.h"

#include <cmath>
#include <numbers>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(TiamatWeakenedDragonAI, "tiamat_weakened_dragon");

// Java TiamatWeakenedDragonAI.java:32-34
TiamatWeakenedDragonAI::TiamatWeakenedDragonAI(Npc& owner) : AggressiveNpcAI(owner) {
	hpPhases.set(gameserver::ai::HpPhases::create(50, {25, 15, 5}));
	hasAggro.set(runtime::Rc<runtime::AtomicBoolean>::create(false));
	spawnTasks.set(runtime::RcArrayList<runtime::FutureRef>::create(AION_LOCK_CLASS(TiamatWeakenedDragonAI::spawnTasks)));
}

// Java TiamatWeakenedDragonAI.java:36-39
ItemAttackType TiamatWeakenedDragonAI::modifyAttackType(ItemAttackType type) {
	return ItemAttackType::MAGICAL_FIRE;
}

// Java TiamatWeakenedDragonAI.java:41-49
void TiamatWeakenedDragonAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (hasAggro.get()->compareAndSet(false, true)) {
		offerAtrocityEvent();
		scheduleSinkingSand();
	}
	hpPhases.get()->tryEnterNextPhase(*this);
}

// Java TiamatWeakenedDragonAI.java:51-66
void TiamatWeakenedDragonAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 50:
			scheduleDivisiveCreations(60000);
			break;
		case 25:
			scheduleInfinitePain();
			spawnGravityCrusher();
			break;
		case 15:
		case 5:
			spawnGravityCrusher();
			break;
	}
}

// Java TiamatWeakenedDragonAI.java:68-74
void TiamatWeakenedDragonAI::offerAtrocityEvent() {
	if (!isDead() && hasAggro.get()->get()) {
		if (getLifeStats()->getHpPercentage() <= 50)
			getOwner().queueSkill(20921, 1, 0);
		getOwner().queueSkill(calculateAtrocitySkillId(), 1);
	}
}

// Java TiamatWeakenedDragonAI.java:76-78
int32_t TiamatWeakenedDragonAI::calculateAtrocitySkillId() {
	return 20922 + (commons::utils::Rnd::nextInt(3) * 2);
}

// Java TiamatWeakenedDragonAI.java:80-104
void TiamatWeakenedDragonAI::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 20922: // Ultimate Atrocity
			spawn(283237, 445.0f, 550.70f, 417.41f, int8_t{0});
			spawn(283241, 469.8f, 543.01f, 417.41f, int8_t{0});
			break;
		case 20924: // Ultimate Atrocity
			spawn(283240, 457.8f, 514.6f, 417.41f, int8_t{0});
			spawn(283240, 462.3f, 514.6f, 417.41f, int8_t{0});
			spawn(283240, 469.7f, 514.6f, 417.41f, int8_t{0});
			spawn(283240, 466.6f, 514.6f, 417.41f, int8_t{0});
			spawn(283240, 473.6f, 514.6f, 417.41f, int8_t{0});
			spawn(283240, 479.3f, 514.6f, 417.41f, int8_t{0});
			spawn(283241, 475.8f, 514.6f, 417.41f, int8_t{0});
			spawn(283241, 491.2f, 514.6f, 417.41f, int8_t{0});
			spawn(283241, 482.7f, 514.6f, 417.41f, int8_t{0});
			spawn(283241, 485.2f, 514.6f, 417.41f, int8_t{0});
			spawn(283241, 488.1f, 514.6f, 417.41f, int8_t{0});
			break;
		case 20926: // Ultimate Atrocity
			spawn(283244, 458.67f, 480.76f, 417.41f, int8_t{0});
			break;
	}
}

// Java TiamatWeakenedDragonAI.java:106-115
void TiamatWeakenedDragonAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 20922: // Ultimate Atrocity
		case 20924:
		case 20926:
			offerAtrocityEvent();
			break;
	}
}

// Java TiamatWeakenedDragonAI.java:117-128
void TiamatWeakenedDragonAI::scheduleSinkingSand() {
	spawnTasks.get()->add(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			int32_t delay = 0;
			for (float degree = -25.0f; degree <= 25.0f; degree += 4) {
				double radian = degree * std::numbers::pi / 180.0; // Math.toRadians
				spawnTasks.get()->add(ThreadPoolManager::getInstance().schedule({this}, [this, radian] { spawnSinkingSand(radian); }, delay += 1800));
			}
			scheduleSinkingSand();
		}
	}, 120000));
}

// Java TiamatWeakenedDragonAI.java:130-138
void TiamatWeakenedDragonAI::spawnSinkingSand(double radian) {
	float dist = 25.0f;
	for (int32_t i = 0; i < 7; i++) {
		dist += 2.5f;
		float x = static_cast<float>(std::cos(radian) * dist);
		float y = static_cast<float>(std::sin(radian) * dist);
		spawn(283135, getPosition()->getX() + x, getPosition()->getY() + y, getPosition()->getZ(), int8_t{0});
	}
}

// Java TiamatWeakenedDragonAI.java:143-153
void TiamatWeakenedDragonAI::scheduleDivisiveCreations(int32_t delay) {
	spawnTasks.get()->add(ThreadPoolManager::getInstance().schedule({this}, [this, delay] {
		if (hasAggro.get()->get() && !isDead() && delay > 30000) {
			spawn(283139, 464.24f, 462.26f, 417.4f, int8_t{18});
			spawn(283139, 542.79f, 465.03f, 417.4f, int8_t{43});
			spawn(283139, 541.79f, 563.71f, 417.4f, int8_t{74});
			spawn(283139, 465.79f, 565.43f, 417.4f, int8_t{100});
			scheduleDivisiveCreations(delay - 10000);
		}
	}, delay));
}

// Java TiamatWeakenedDragonAI.java:155-160
void TiamatWeakenedDragonAI::spawnGravityCrusher() {
	spawn(283141, 464.24f, 462.26f, 417.4f, int8_t{18});
	spawn(283141, 542.79f, 465.03f, 417.4f, int8_t{43});
	spawn(283141, 541.79f, 563.71f, 417.4f, int8_t{74});
	spawn(283141, 465.79f, 565.43f, 417.4f, int8_t{100});
}

// Java TiamatWeakenedDragonAI.java:162-169
void TiamatWeakenedDragonAI::scheduleInfinitePain() {
	spawnTasks.get()->add(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (hasAggro.get()->get() && !isDead()) {
			spawnInfinitePain();
			scheduleInfinitePain();
		}
	}, 90000));
}

// Java TiamatWeakenedDragonAI.java:171-173
void TiamatWeakenedDragonAI::spawnInfinitePain() {
	spawn(283143, 508.32f, 515.18f, 417.4f, int8_t{0});
}

// Java TiamatWeakenedDragonAI.java:175-180
void TiamatWeakenedDragonAI::despawnAdds() {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	deleteNpcs(instance->getNpcs({283141}));
	deleteNpcs(instance->getNpcs({283139}));
	deleteNpcs(instance->getNpcs({283140}));
}

// Java TiamatWeakenedDragonAI.java:182-186
void TiamatWeakenedDragonAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs)
		if (npc != nullptr)
			npc->getController().delete_();
}

// Java TiamatWeakenedDragonAI.java:188-192
void TiamatWeakenedDragonAI::cancelTasks() {
	for (const runtime::FutureRef& task : *spawnTasks.get())
		if (task != nullptr && !task->isCancelled())
			task->cancel(true);
}

// Java TiamatWeakenedDragonAI.java:194-200
void TiamatWeakenedDragonAI::handleDied() {
	cancelTasks();
	despawnAdds();
	services::RespawnService::scheduleDecayTask(getOwner(), 3000);
	AggressiveNpcAI::handleDied();
}

// Java TiamatWeakenedDragonAI.java:202-207
void TiamatWeakenedDragonAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	despawnAdds();
	cancelTasks();
}

// Java TiamatWeakenedDragonAI.java:209-216
void TiamatWeakenedDragonAI::handleBackHome() {
	cancelTasks();
	hasAggro.get()->set(false);
	despawnAdds();
	AggressiveNpcAI::handleBackHome();
	hpPhases.get()->reset();
}

// Java TiamatWeakenedDragonAI.java:218-225
bool TiamatWeakenedDragonAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
			return false;
		case AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKED:
		case AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKING:
			return true;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
