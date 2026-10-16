#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/OneDmgAI.h"

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::worlds {

/**
 * The seasonal Agrints ("agrint"): the spawn announces the season to the map; at 50 % HP five helpers (npc id + 320, or + 308 for the second
 * set) appear around it, back home the phase starts again; its death leaves six chests of its season.
 * <p>
 * Java: data/handlers/ai/worlds/AgrintAI.java, @AIName("agrint") (the marker is in the .cpp).
 *
 * @author xTz, Neon
 */
class AgrintAI : public OneDmgAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(50); // Java: = new HpPhases(50)

public:
	explicit AgrintAI(Npc& owner) : OneDmgAI(owner) {}

protected:
	void handleSpawned() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

protected:
	void handleBackHome() override;

private:
	void spawnChests(int32_t npcId);

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::worlds
