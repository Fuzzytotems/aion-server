#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include <initializer_list>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of the circus boxes ("circusboxes"): 33 s after its spawn a living box goes and lets out 3 to 5 jesters (the terror's or the
 * nightmare's); its despawn takes the jesters, its death is announced.
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/CircusBoxesAI.java, @AIName("circusboxes") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class CircusBoxesAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> spawnJesterTask{};

public:
	explicit CircusBoxesAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDespawned() override;

	void handleDied() override;

private:
	void cancelspawnJesterTask();

	void spawnJester();

	void despawnNpcs(std::initializer_list<int32_t> npcIds);
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
