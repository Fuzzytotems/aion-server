#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Chantra's traps in Tiamat Stronghold ("chantrarings"): a player who moves into its band (283172: 10-18 m, 283171: 0-10 and 18-25 m)
 * gets its debuff (20735 for 283172, 20734 for the others) unless he has it; it is deleted 20 s after its spawn. No decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/ChantraRingsAI.java, @AIName("chantrarings") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class ChantraRingsAI : public NpcAI {
public:
	explicit ChantraRingsAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

private:
	void checkDistance(NpcAI& ai, Creature& creature);

protected:
	void handleSpawned() override;

private:
	void despawn();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
