#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/siege/SiegeNpcAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The base AI of a siege protector (the boss): back home it forgets its aggro list; its death credits the aggro list damage to the attackers'
 * races, marks the boss killed and stops the siege.
 * <p>
 * Java: data/handlers/ai/siege/AbstractSiegeProtectorAI.java. M5i I-2, an A1 lease.
 */
class AbstractSiegeProtectorAI : public SiegeNpcAI {
public:
	explicit AbstractSiegeProtectorAI(Npc& owner) : SiegeNpcAI(owner) {}

	/** Java widens the visibility to public */
	void handleBackHome() override;

	/** Java: static, package-private (EnragedAgent calls it) */
	static void stopSiege(SiegeNpc& siegeProtector);

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::siege
