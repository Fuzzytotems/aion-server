#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the seal breakers ("drakenspire_seal_breaker"): it shouts as it dies; immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/SealBreakerAI.java, @AIName("drakenspire_seal_breaker") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class SealBreakerAI : public AggressiveNoLootNpcAI {
public:
	explicit SealBreakerAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

protected:
	void handleDied() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
