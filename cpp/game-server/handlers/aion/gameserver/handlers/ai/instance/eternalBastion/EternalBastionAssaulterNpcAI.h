#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of Eternal Bastion's assaulters ("eternal_bastion_assaulter"): three seconds after the spawn they run their route; at its end (and back
 * home) they hate the fortress commander within 20 m.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionAssaulterNpcAI.java, @AIName("eternal_bastion_assaulter") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Estrayl
 */
class EternalBastionAssaulterNpcAI : public EternalBastionAggressiveNpcAI {
public:
	explicit EternalBastionAssaulterNpcAI(Npc& owner) : EternalBastionAggressiveNpcAI(owner) {}

private:
	/** Java: private int getAggroRange() - never called */
	int32_t getAggroRange();

protected:
	void handleSpawned() override;

private:
	void startWalking();

public:
	bool isDestinationReached() override;

private:
	void hateCommander();

protected:
	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
