#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::quests {

/**
 * The AI of Garnon in quest 20060 ("Q20060"): three minutes after its spawn it spawns 800020 and goes.
 * <p>
 * Java: data/handlers/ai/quests/GarnonQ20060AI.java, @AIName("Q20060") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class GarnonQ20060AI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit GarnonQ20060AI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDespawned() override;

	void handleSpawned() override;

private:
	void despawn();
};

} // namespace aion::gameserver::handlers::ai::quests
