#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the twins' door destroyer ("twin_door_destroyer"): spawned, it removes the traps (207128, 207129) after 1.5 s and shouts;
 * at the end of its walk it shouts and 3.5 s later blows up the gates (731580) within 10 m.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/TwinDoorDestroyerAI.java, @AIName("twin_door_destroyer") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class TwinDoorDestroyerAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isGateReached{AION_LOCK_CLASS(TwinDoorDestroyerAI::isGateReached)}; // Java: = new AtomicBoolean()

public:
	explicit TwinDoorDestroyerAI(Npc& owner) : GeneralNpcAI(owner) {}

	void handleSpawned() override;

	void handleMoveArrived() override;

private:
	void removeTrap();

	void scheduleGateAttack();
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
