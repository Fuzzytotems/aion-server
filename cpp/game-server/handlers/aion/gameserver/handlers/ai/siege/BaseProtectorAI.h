#pragma once

#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/model/templates/spawns/basespawns/fwd.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The boss of a base ("base_protector"): its death makes the race (or Panesterra faction) of the most damaging attacker - a temporary team
 * counts as its leader - the new occupier of the base; at level 65 and above its maximum hp takes the base protector multiplier of SiegeConfig.
 * <p>
 * Java: data/handlers/ai/siege/BaseProtectorAI.java, @AIName("base_protector") (the marker is in the .cpp). M5i I-8, under P5-12b1's lease.
 *
 * @author Estrayl
 */
class BaseProtectorAI : public AggressiveNpcAI {
public:
	explicit BaseProtectorAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void modifyOwnerStat(Stat2& stat) override;

	bool ask(AIQuestion question) override;

protected:
	/** Java's covariant override: NpcAI::getSpawnTemplate narrowed to the BaseSpawnTemplate (a cast, hub-headers.md §8.2) */
	runtime::Ptr<model::templates::spawns::basespawns::BaseSpawnTemplate> getSpawnTemplate();

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::siege
