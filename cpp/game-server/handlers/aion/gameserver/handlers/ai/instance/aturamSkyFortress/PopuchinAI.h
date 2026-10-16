#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

/**
 * The AI of Popuchin ("popuchin"): attacked, it closes the door and every 15.5 s throws a bomb at its target, then spawns two bombs at
 * its place (above half hp) or ten around it; back home or dead it opens the door; immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/aturamSkyFortress/PopuchinAI.java, @AIName("popuchin") (the marker is in the .cpp).
 *
 * @author xTz
 */
class PopuchinAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> isHome{true};
	runtime::Field<runtime::FutureRef> bombTask{};

public:
	explicit PopuchinAI(Npc& owner) : AggressiveNpcAI(owner) {}

private:
	void startBombTask();

public:
	void handleAttack(runtime::Ptr<Creature> creature) override;

protected:
	void handleBackHome() override;

private:
	void spawnRndBombs();

public:
	bool ask(AIQuestion question) override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
