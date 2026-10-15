#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of the gates' summons in the Unstable Splinterpath ("summonedunstablelapilima"): they walk to Yamennes (219563) without thinking
 * and, arrived, heal or shield her every 30 s while she lives; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/UnstableGatesSummonedAI.java, @AIName("summonedunstablelapilima") (the marker is in the .cpp).
 *
 * @author Ritsu, Cheatkiller
 */
class UnstableGatesSummonedAI : public GeneralNpcAI {
private:
	runtime::Field<runtime::FutureRef> eventTask{};
	runtime::Field<bool> canThink_{true}; // Java: canThink (the name clashes with canThink())

public:
	explicit UnstableGatesSummonedAI(Npc& owner) : GeneralNpcAI(owner) {}

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

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
