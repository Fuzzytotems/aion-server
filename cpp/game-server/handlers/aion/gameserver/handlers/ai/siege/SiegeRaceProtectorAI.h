#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/siege/SiegeNpcAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * A race protector of a siege location ("siege_raceprotector"): unlike the other siege npcs it decays and rewards ap, xp, dp and loot.
 * <p>
 * Java: data/handlers/ai/siege/SiegeRaceProtectorAI.java, @AIName("siege_raceprotector") (the marker is in the .cpp). M5i I-2, an A1 lease.
 *
 * @author Source
 */
class SiegeRaceProtectorAI : public SiegeNpcAI {
public:
	explicit SiegeRaceProtectorAI(Npc& owner) : SiegeNpcAI(owner) {}

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::siege
