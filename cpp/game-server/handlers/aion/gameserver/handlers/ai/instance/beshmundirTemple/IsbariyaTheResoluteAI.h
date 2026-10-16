#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"

#include <initializer_list>

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of Isbariya the Resolute ("isbariya"): attacked, she shouts, closes door 535 and casts 18912 or 18913 every 24 s; from 75 % hp
 * her specials: souls and 18959 (phase 1, 25 s), five 281660 (phase 2, 10 s), a 281659 and 18993 (phase 3, 20 s); home or dead she opens
 * the door.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/IsbariyaTheResoluteAI.java, @AIName("isbariya") (the marker is in the .cpp).
 *
 * @author unknown
 */
class IsbariyaTheResoluteAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75, {50, 25}); // Java: = new HpPhases(75, 50, 25)
	runtime::AtomicBoolean isStart{AION_LOCK_CLASS(IsbariyaTheResoluteAI::isStart)}; // Java: = new AtomicBoolean()
	runtime::ArrayList<runtime::Ref<Point3D>> soulLocations{AION_LOCK_CLASS(IsbariyaTheResoluteAI::soulLocations)}; // Java: = new ArrayList<>()
	runtime::Field<runtime::FutureRef> basicSkillTask{};
	runtime::Field<runtime::FutureRef> spawnTask{};

public:
	explicit IsbariyaTheResoluteAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

protected:
	void handleSpawned() override;

	void handleDied() override;

	void handleDespawned() override;

	void handleBackHome() override;

private:
	void startBasicSkillTask();

	void launchSpecial();

	void rndSpawn(int32_t npcId, int32_t count);

	void spawnSouls();

	runtime::Ptr<Creature> getRandomTarget();

	void scheduleSpecial(int32_t delay);

	static void cancelTasks(std::initializer_list<runtime::FutureRef> tasks);
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
