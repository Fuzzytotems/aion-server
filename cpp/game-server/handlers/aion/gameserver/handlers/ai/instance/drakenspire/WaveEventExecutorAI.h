#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the wave event's executor ("wave_event_executor"): at the end of its walk it shouts and 3.5 s later summons (20839).
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/WaveEventExecutorAI.java, @AIName("wave_event_executor") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class WaveEventExecutorAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isDestinationReached{AION_LOCK_CLASS(WaveEventExecutorAI::isDestinationReached)}; // Java: = new AtomicBoolean()

public:
	explicit WaveEventExecutorAI(Npc& owner) : GeneralNpcAI(owner) {}

	void handleMoveArrived() override;

private:
	void scheduleSummon();
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
