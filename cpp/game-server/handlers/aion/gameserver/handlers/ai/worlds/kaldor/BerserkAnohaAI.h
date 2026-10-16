#pragma once

#include <optional>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::worlds::kaldor {

/**
 * The AI of Berserk Anoha in Kaldor ("berserk_anoha"): her spawn tells the world, keeps the race holding Anoha's fortress (7011) and despawns
 * her after an hour unless she dies (the world is told when she leaves alive). A player she sees gets the quest of his race and the
 * occupier (13817/13818, 23817/23818) when it is startable. Her death tells the world and spawns the occupier's Commander Anoha
 * (804594 for the Asmodians, 804595 otherwise) for an hour; dead or despawned she takes the map icon (702618) away. No decay, no respawn,
 * no loot.
 * <p>
 * Java: data/handlers/ai/worlds/kaldor/BerserkAnohaAI.java, @AIName("berserk_anoha") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl
 */
class BerserkAnohaAI : public AggressiveNpcAI {
private:
	runtime::Field<std::optional<SiegeRace>> occupier{}; // Java: null until handleSpawned

public:
	explicit BerserkAnohaAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void scheduleDespawn();

protected:
	void handleDespawned() override;

	void handleDied() override;

private:
	void despawnFlag();

	void checkForFactionReward();

protected:
	void handleCreatureSee(Creature& creature) override;

public:
	bool ask(AIQuestion question) override;

private:
	void startQuest(Player& player, int32_t questId);
};

} // namespace aion::gameserver::handlers::ai::worlds::kaldor
