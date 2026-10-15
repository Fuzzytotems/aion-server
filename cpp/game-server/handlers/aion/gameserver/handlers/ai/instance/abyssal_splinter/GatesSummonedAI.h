#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

/**
 * The AI of the gates' summons in the Abyssal Splinter ("gatessummoned"): they walk to Yamennes Painflare (216960) without thinking and,
 * arrived, heal or shield her every 30 s while she lives; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/abyssal_splinter/GatesSummonedAI.java, @AIName("gatessummoned") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class GatesSummonedAI : public GeneralNpcAI {
private:
	runtime::Field<runtime::FutureRef> eventTask{};
	runtime::Field<bool> canThink_{true}; // Java: canThink (the name clashes with canThink())

public:
	explicit GatesSummonedAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	bool canThink() override;

protected:
	void handleDied() override;

	void handleDespawned() override;

	void handleMoveArrived() override;

private:
	void startMove();

	void cancelEventTask();

	void startEventTask();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
