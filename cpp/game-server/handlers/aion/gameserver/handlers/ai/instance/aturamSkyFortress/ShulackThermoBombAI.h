#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

/**
 * The AI of the Shulack thermo bombs ("shulack_thermo_bomb"): 2 s after the spawn it explodes (19416) and goes 4 s later; no decay,
 * respawn or rewards; immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/aturamSkyFortress/ShulackThermoBombAI.java, @AIName("shulack_thermo_bomb") (the marker is in the .cpp).
 *
 * @author xTz
 */
class ShulackThermoBombAI : public AggressiveNpcAI {
public:
	explicit ShulackThermoBombAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void doSchedule();

public:
	bool ask(AIQuestion question) override;

private:
	void despawn();
};

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
