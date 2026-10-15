#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

/**
 * The AI of Queen Mosqua's eggs ("mosquaegg"): 17 s after its spawn an egg hatches a Supraklaw and goes; no decay, no rewards.
 * <p>
 * Java: data/handlers/ai/instance/tallocsHollow/MosquaEggAI.java, @AIName("mosquaegg") (the marker is in the .cpp).
 *
 * @author xTz, Sykra
 */
class MosquaEggAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::FutureRef> supraklawSpawnTask{};

public:
	explicit MosquaEggAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDespawned() override;

	void handleDied() override;

public:
	bool ask(AIQuestion question) override;

private:
	void cancelSpawnTask();
};

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
