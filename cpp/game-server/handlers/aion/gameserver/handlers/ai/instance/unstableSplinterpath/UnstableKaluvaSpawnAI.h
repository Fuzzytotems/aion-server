#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of Kaluva's eggs in the Unstable Splinterpath ("unstablekaluvaspawn"): it casts 19222 on its spawn; Kaluva coming within 7 m of
 * it loses 19152 and starts the hatch, 28 s later one of four add formations; hatched or killed it takes Kaluva's 19152 and goes; no
 * loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/UnstableKaluvaSpawnAI.java, @AIName("unstablekaluvaspawn") (the marker is in the .cpp).
 */
class UnstableKaluvaSpawnAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit UnstableKaluvaSpawnAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDied() override;

	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

private:
	void onMoved(Creature& creature);

protected:
	void handleSpawned() override;

private:
	void checkKaluva();

	void scheduleHatch();

	void hatchAdds();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
