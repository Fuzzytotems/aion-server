#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the wave event's bomber ("wave_event_bomber"): spawned, it shouts 4 s later.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/WaveEventBomberAI.java, @AIName("wave_event_bomber") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class WaveEventBomberAI : public GeneralNpcAI {
public:
	explicit WaveEventBomberAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
