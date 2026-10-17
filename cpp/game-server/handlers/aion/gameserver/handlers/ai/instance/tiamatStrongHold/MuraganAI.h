#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Muragan and his men in Tiamat Stronghold ("muragan", 800435-800438): 800438 speaks 1 s after its spawn; a player within 15 m
 * sets one off once: 800436 speaks, opens Surama's door (56) and goes; 800435 speaks, turns the guard captains (219392) into 283145 and,
 * with the others but 800438, walks off and is deleted 10 s later.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/MuraganAI.java, @AIName("muragan") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class MuraganAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isInMove{AION_LOCK_CLASS(MuraganAI::isInMove), false}; // Java: = new AtomicBoolean(false)

public:
	explicit MuraganAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

private:
	void checkDistance(Creature& creature);

	void startWalk();

	void openSuramaDoor();

	void killGuardCaptain();
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
