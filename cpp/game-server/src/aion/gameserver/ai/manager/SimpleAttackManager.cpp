#include "aion/gameserver/ai/manager/SimpleAttackManager.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AggroTarget.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::ai::manager {

using event::AIEventType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using utils::PositionUtil;

// Java implements Runnable. A task object of schedule that reads npcAI on the pool thread and nulls it in run(), so it is RefCounted and
// retains the AI part (fieldmap K4, hub-headers.md §7.3/§9.3). The null-out is Java's own cycle breaker (SimpleAttackManager.java:109) and is
// kept: the Future releases the task when it has run, and the task releases the AI.
class SimpleAttackManager::SimpleCheckedAttackAction final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<NpcAI>> npcAI{};

protected:
	explicit SimpleCheckedAttackAction(NpcAI& value) : npcAI(runtime::Ref<NpcAI>(value)) {}
	~SimpleCheckedAttackAction() override = default;

public:
	/** Java: new SimpleCheckedAttackAction(npcAI) */
	static runtime::Ref<SimpleCheckedAttackAction> create(NpcAI& npcAI) { return runtime::makeRef<SimpleCheckedAttackAction>(npcAI); }

	void run() { // @Override of a Java library type
		runtime::Ptr<NpcAI> ai = npcAI.get(); // Java: NullPointerException once npcAI is null
		if (!ai->getOwner().getGameStats()->isNextAttackScheduled()) {
			attackAction(*ai);
		} else {
			if (ai->isLogging()) {
				AILogger::info(*ai, "Scheduled checked attacked confirmed");
			}
		}
		npcAI.set(nullptr);
	}
};

void SimpleAttackManager::performAttack(NpcAI& npcAI, int32_t delay) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "performAttack");
	}
	// java-race: isNextAttackScheduled() and setNextAttackTime() are a check-then-act across threads (NpcGameStats.java:123-138), and that race
	// is how Java avoids double attacks at all - SimpleCheckedAttackAction below is its own mitigation (m5b-plan.md §8 risk 10). Kept as written.
	if (npcAI.getOwner().getGameStats()->isNextAttackScheduled()) {
		if (npcAI.isLogging()) {
			AILogger::info(npcAI, "Attack already scheduled");
		}
		scheduleCheckedAttackAction(npcAI, delay);
		return;
	}

	npcAI.getOwner().getGameStats()->setNextAttackTime(commons::utils::currentTimeMillis() + delay);
	if (delay > 0) {
		utils::ThreadPoolManager::getInstance().schedule({&npcAI}, [&npcAI] { attackAction(npcAI); }, delay);
	} else {
		attackAction(npcAI);
	}
}

void SimpleAttackManager::scheduleCheckedAttackAction(NpcAI& npcAI, int32_t delay) {
	if (delay < 2000) {
		delay = 2000;
	}
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "Scheduling checked attack " + std::to_string(delay));
	}
	runtime::Ref<SimpleCheckedAttackAction> action = SimpleCheckedAttackAction::create(npcAI);
	utils::ThreadPoolManager::getInstance().schedule({&npcAI}, [action] { action->run(); }, delay);
}

bool SimpleAttackManager::isTargetInAttackRange(Npc& npc) {
	runtime::Ptr<VisibleObject> target = npc.getTarget();
	runtime::Ptr<Creature> creature = runtime::as<Creature>(target);
	if (!creature)
		return false;
	return PositionUtil::isInAttackRange(runtime::Ptr<Creature>(npc), creature, npc.getGameStats()->getAttackRange()->getCurrent() / 1000.0f);
}

void SimpleAttackManager::attackAction(NpcAI& npcAI) {
	if (!npcAI.isInState(AIState::FIGHT)) {
		return;
	}
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "attackAction");
	}
	Npc& npc = npcAI.getOwner();
	runtime::Ptr<Creature> mostHated = npc.getAggroList().getTarget(controllers::attack::AggroTarget::MOST_HATED);
	// Java: mostHated.equals(npc.getTarget()) - one read of the target field, and equals(null) is false (AionObject.java:57-58)
	runtime::Ptr<VisibleObject> currentTarget = npc.getTarget();
	if (mostHated && !(currentTarget && mostHated->equals(*currentTarget))) {
		npcAI.onCreatureEvent(AIEventType::TARGET_CHANGED, *mostHated);
	} else if (runtime::Ptr<Creature> target = runtime::as<Creature>(npc.getTarget()); !target || target->isDead()) {
		npcAI.onGeneralEvent(AIEventType::TARGET_GIVEUP);
	} else if (!npc.canSee(target)) {
		npc.getController().abortCast();
		npcAI.onGeneralEvent(AIEventType::TARGET_TOOFAR);
	} else if (!isTargetInAttackRange(npc)) {
		npcAI.onGeneralEvent(AIEventType::TARGET_TOOFAR);
	} else if (!world::geo::GeoService::getInstance().canSee(npc, *target)) { // delete geo check when we've implemented a pathfinding system
		npc.getController().cancelCurrentSkill(nullptr);
		if ((commons::utils::currentTimeMillis() - npc.getMoveController()->getLastMoveUpdate()) > 15000 &&
			npc.getGameStats()->getLastAttackedTimeDelta() > 15) {
			npcAI.onGeneralEvent(AIEventType::TARGET_GIVEUP);
		} else {
			npcAI.onGeneralEvent(AIEventType::ATTACK_COMPLETE);
		}
	} else {
		if (npc.isSpawned() && !npc.isDead() && !npc.getLifeStats()->isAboutToDie() && npc.canAttack()) {
			npc.getPosition()->setH(PositionUtil::getHeadingTowards(npc, *target));
			npc.getController().attackTarget(target, 0, true);
		}
		npcAI.onGeneralEvent(AIEventType::ATTACK_COMPLETE);
	}
}

} // namespace aion::gameserver::ai::manager
