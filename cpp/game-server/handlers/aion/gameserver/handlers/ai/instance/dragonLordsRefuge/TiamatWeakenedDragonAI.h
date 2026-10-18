#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/Rc.h"

#include <vector>

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the weakened Tiamat ("tiamat_weakened_dragon"): fire; her first hit starts the atrocities (one after the other; at half hp
 * 20921 first) and the sinking sand every 2 minutes; at 50 % the divisive creations (60, 50, 40 s apart), at 25 % the infinite pain
 * every 90 s, at 25, 15 and 5 % the gravity crushers; death, despawn and home take the adds and stop the tasks; no loot.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/TiamatWeakenedDragonAI.java, @AIName("tiamat_weakened_dragon") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Estrayl
 */
class TiamatWeakenedDragonAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
protected:
	runtime::Field<runtime::Ref<gameserver::ai::HpPhases>> hpPhases{}; // Java: = new HpPhases(50, 25, 15, 5) (the HM form replaces it)
	runtime::Field<runtime::Ref<runtime::Rc<runtime::AtomicBoolean>>> hasAggro{}; // Java: = new AtomicBoolean()
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::FutureRef>>> spawnTasks{}; // Java: = new ArrayList<>()

public:
	explicit TiamatWeakenedDragonAI(Npc& owner);

	ItemAttackType modifyAttackType(ItemAttackType type) override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void offerAtrocityEvent();

protected:
	virtual int32_t calculateAtrocitySkillId();

public:
	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	virtual void scheduleSinkingSand();

private:
	void spawnSinkingSand(double radian);

protected:
	virtual void scheduleDivisiveCreations(int32_t delay);

	virtual void spawnGravityCrusher();

	void scheduleInfinitePain();

	virtual void spawnInfinitePain();

	virtual void despawnAdds();

	static void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);

private:
	void cancelTasks();

protected:
	void handleDied() override;

	void handleDespawned() override;

	void handleBackHome() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
