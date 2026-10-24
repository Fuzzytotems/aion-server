#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the gravity tornadoes ("gravity_tornado"): from 2.5 s every 6 s it casts 20966 (283142) or 21901; no decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/GravityTornadoAI.java, @AIName("gravity_tornado") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class GravityTornadoAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit GravityTornadoAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void handleDespawned() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
