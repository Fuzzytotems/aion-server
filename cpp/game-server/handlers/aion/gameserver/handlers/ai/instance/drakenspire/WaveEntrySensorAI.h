#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the wave event's entry sensor ("wave_entry_sensor"): it goes when it detects a player.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/WaveEntrySensorAI.java, @AIName("wave_entry_sensor") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class WaveEntrySensorAI : public AggressiveNpcAI {
public:
	explicit WaveEntrySensorAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void handleCreatureDetected(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
