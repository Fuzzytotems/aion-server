#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/siege/AbstractSiegeProtectorAI.h"

#include <cstdint>
#include <string_view>

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The two empowered agents of the agent fight, Veille 235064 and Mastarius 235065 ("empowered_agent"). An agent spawns shielded by 21779,
 * thinking nothing, and walks its route to the middle; there it plants its flag and, once the other agent is within 5 m, drops the shield
 * and fights it. Each hp phase (80 to 20 %) brings Balaur guards of its kind, 25 and 5 % queue 21778; every 15 s players lose their hate.
 * Its death tells the AgentSiege the other race won.
 * <p>
 * Java: data/handlers/ai/siege/EmpoweredAgent.java, @AIName("empowered_agent") (the marker is in the .cpp). C++: the activation and aggro
 * reset tasks are pinned to this AI.
 *
 * @author Estrayl
 */
class EmpoweredAgent : public AbstractSiegeProtectorAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	runtime::ArrayList<int32_t> guardIds{AION_LOCK_CLASS(EmpoweredAgent::guardIds)}; // Java: = new ArrayList<>()
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(80, {70, 60, 50, 40, 30, 25, 20, 5});
	runtime::AtomicBoolean isWalkingCompleted{AION_LOCK_CLASS(EmpoweredAgent::isWalkingCompleted)}; // Java: = new AtomicBoolean()
	runtime::Field<bool> canThink_{true}; // Java: boolean canThink (the method shares its name)
	runtime::Field<runtime::Ref<Npc>> flagNpc{};
	runtime::Field<runtime::FutureRef> activationTask{};
	runtime::Field<runtime::FutureRef> aggroResetTask{};

public:
	explicit EmpoweredAgent(Npc& owner) : AbstractSiegeProtectorAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

private:
	void startWalking(std::string_view walkerId);

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

protected:
	void handleMoveArrived() override;

private:
	void onDestinationArrived();

	void tryActivating();

	runtime::Ptr<Npc> getOtherAgent();

	void startFight(Npc& otherAgent);

	void resetPlayerHate();

	void spawnFlag();

	void despawnFlag();

	void onGuardSpawnEvent();

	runtime::Ref<Point3D> getRndPos();

	void cancelActivationTask();

protected:
	void handleDied() override;

	void handleDespawned() override;

public:
	bool ask(AIQuestion question) override;

	void modifyOwnerStat(Stat2& stat) override;
};

} // namespace aion::gameserver::handlers::ai::siege
