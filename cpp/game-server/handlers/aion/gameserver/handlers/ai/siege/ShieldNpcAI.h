#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/siege/SiegeNpcAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The shield generator of a fortress ("siege_shieldnpc"): its spawn raises the fortress shield and its despawn drops it, each announced to the
 * map with SM_SHIELD_EFFECT; a field stone (race CONSTRUCT) does not think, so it does not reset.
 * <p>
 * Java: data/handlers/ai/siege/ShieldNpcAI.java, @AIName("siege_shieldnpc") (the marker is in the .cpp). M5i I-2, an A1 lease.
 *
 * @author Source
 */
class ShieldNpcAI : public SiegeNpcAI {
public:
	explicit ShieldNpcAI(Npc& owner) : SiegeNpcAI(owner) {}

	bool canThink() override;

	bool ask(AIQuestion question) override;

protected:
	void handleDespawned() override;

	void handleSpawned() override;

private:
	void updateFortressShieldStatus(bool hasShield);
};

} // namespace aion::gameserver::handlers::ai::siege
