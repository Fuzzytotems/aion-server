#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of the walking talking bombs on the Steel Rake ("walkingtalkingbomb"): 2 s after its spawn, alive, it casts 19416 on itself and 4 s
 * later, still alive, is deleted; no decay, respawn or rewards, immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/rakes/WalkingTalkingBombAI.java, @AIName("walkingtalkingbomb") (the marker is in the .cpp).
 */
class WalkingTalkingBombAI : public AggressiveNpcAI {
public:
	explicit WalkingTalkingBombAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void doSchedule();

public:
	bool ask(AIQuestion question) override;

private:
	void despawn();
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
