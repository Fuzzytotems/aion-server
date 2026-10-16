#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * The base of Ahserion's Flight's constructs (no name: Java has no @AIName on it): max hp and the four siege skills' damage of npcs scale with
 * gameserver.siege.ahserion.max.players.per.team / 100; no respawn, no loot.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionConstructAI.java.
 *
 * @author Yeats, Estrayl
 */
class AhserionConstructAI : public NpcAI {
public:
	explicit AhserionConstructAI(Npc& owner) : NpcAI(owner) {}

	void modifyOwnerStat(Stat2& stat) override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
