#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Shabokan's earthquake in Tiamat Stronghold ("earthquake"): a player within 5 m gets 20718 unless he has it; it is deleted 9 s
 * after its spawn. No decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/EarthQuakeAI.java, @AIName("earthquake") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class EarthQuakeAI : public NpcAI {
public:
	explicit EarthQuakeAI(Npc& owner) : NpcAI(owner) {}

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
