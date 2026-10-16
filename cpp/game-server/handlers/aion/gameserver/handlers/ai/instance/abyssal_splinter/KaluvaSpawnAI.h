#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

/**
 * The AI of Kaluva's egg in the Abyssal Splinter ("kaluvaspawn"): 22 s after its spawn (when Kaluva's debuff ends) it hatches one of four
 * add formations; hatched or killed it takes Kaluva's 19152 and goes; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/abyssal_splinter/KaluvaSpawnAI.java, @AIName("kaluvaspawn") (the marker is in the .cpp).
 */
class KaluvaSpawnAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit KaluvaSpawnAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDied() override;

	void handleSpawned() override;

private:
	void checkKaluva();

	void scheduleHatch();

	void hatchAdds();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
