#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::classNpc {

/**
 * The AI of the enemy servants ("enemyservant"): two seconds after the spawn it targets its creator's target and attacks it with skill
 * 16907 every 6 s; no decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/classNpc/EnemyServantAI.java, @AIName("enemyservant") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class EnemyServantAI : public NpcAI {
public:
	explicit EnemyServantAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void attack();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::classNpc
