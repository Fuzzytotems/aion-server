#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

#include <cstdint>

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the wave defenders ("wave_defender"): 3 s after the end of its walk it is replaced by its race's standing defender (236248,
 * 236249) facing the walk's heading.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/WaveDefenderAI.java, @AIName("wave_defender") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class WaveDefenderAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isDestinationReached{AION_LOCK_CLASS(WaveDefenderAI::isDestinationReached)}; // Java: = new AtomicBoolean()

public:
	explicit WaveDefenderAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void handleMoveArrived() override;

private:
	void scheduleLocationUpdate();

	int8_t getHeading();
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
