#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <vector>

namespace aion::gameserver::handlers::ai::events {

/**
 * The AI of the Silentera RvR event bosses ("rvr_boss"): attackers in Silentera Canyon join the event's reward list; a boss's death
 * removes the other, respawns both after 12 hours and mails the listed players a medal (from 19:00 to 23:59).
 * <p>
 * Java: data/handlers/ai/events/RvrBossAI.java, @AIName("rvr_boss") (the marker is in the .cpp).
 *
 * @author Bobobear
 */
class RvrBossAI : public AggressiveNpcAI {
public:
	explicit RvrBossAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleDied() override;

private:
	void despawnEnemyBoss();

	void scheduleRespawn();

	void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);

	void sendParticipationRewards();

protected:
	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::events
