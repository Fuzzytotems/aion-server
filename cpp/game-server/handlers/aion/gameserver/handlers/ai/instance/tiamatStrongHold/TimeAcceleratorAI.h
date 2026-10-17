#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Kumbanda's time accelerators in Tiamat Stronghold ("timeaccelerator"): any creature within 5 m gets 20727 unless it has it; it is
 * deleted 20 s after its spawn. No decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/TimeAcceleratorAI.java, @AIName("timeaccelerator") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class TimeAcceleratorAI : public NpcAI {
public:
	explicit TimeAcceleratorAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

private:
	void checkDistance(Creature& creature);

protected:
	void handleSpawned() override;

private:
	void despawn();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
