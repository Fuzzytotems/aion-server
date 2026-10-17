#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of the monolithic ambushers ("monolithicambusher"): the first aggro calls the living 216215 of its row (the spawn's y) within
 * 60 m; back home it may call again.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/MonolithicAmbusherAI.java, @AIName("monolithicambusher") (the marker is in the .cpp).
 *
 * @author unknown
 */
class MonolithicAmbusherAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> hasHelped{};

public:
	explicit MonolithicAmbusherAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleBackHome() override;

	void handleCreatureAggro(Creature& creature) override;

private:
	void help(Creature& creature);
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
