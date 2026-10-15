#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of the enraged nightmares ("enragednightmare"): spawned they walk to the circus' middle (from the left or the right point); seeing
 * anything while Heiramune is hurt they heal him and go 3 s later; they never go home.
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/EnragedNightmareAI.java, @AIName("enragednightmare") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class EnragedNightmareAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::FutureRef> skillTask{};
	runtime::Field<runtime::Ref<Npc>> boss{};

public:
	explicit EnragedNightmareAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

private:
	void cancelTask();

protected:
	void handleDied() override;

	void handleDespawned() override;

	void handleSpawned() override;

private:
	void useSkill(gameserver::ai::NpcAI& ai, runtime::Ptr<Creature> creature);

protected:
	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
