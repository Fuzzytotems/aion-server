#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionConstructAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * A team's advance corridor shield in Ahserion's Flight ("ahserion_advance_corridor_shield"): under attack it tells the players of 400030000 at
 * most every 3 s; its death during the raid announces the break and eliminates its team (AhserionRaid.handleCorridorShieldDestruction).
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AdvanceCorridorShield.java, @AIName("ahserion_advance_corridor_shield") (the marker
 * is in the .cpp). C++: the shout reset is pinned to this AI.
 *
 * @author Yeats
 */
class AdvanceCorridorShield : public AhserionConstructAI {
private:
	runtime::AtomicBoolean canShout{AION_LOCK_CLASS(AdvanceCorridorShield::canShout), true}; // Java: = new AtomicBoolean(true)

public:
	explicit AdvanceCorridorShield(Npc& owner) : AhserionConstructAI(owner) {}

	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void broadcastAttack();

	void allowShout();

	void sendPacket(network::aion::serverpackets::SM_SYSTEM_MESSAGE msg);

public:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
