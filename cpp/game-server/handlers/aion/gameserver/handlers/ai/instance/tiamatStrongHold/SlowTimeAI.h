#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of the slowed time in Tiamat Stronghold ("slowedtime"): a player within 50 m gets 20728 unless he has it. No decay, respawn or
 * rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/SlowTimeAI.java, @AIName("slowedtime") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class SlowTimeAI : public NpcAI {
public:
	explicit SlowTimeAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

private:
	void checkDistance(NpcAI& ai, Creature& creature);

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
