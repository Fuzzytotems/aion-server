#include "aion/gameserver/handlers/ai/HomingNpcAI.h"

namespace aion::gameserver::handlers::ai {

AION_AI(HomingNpcAI, "homing");

// Java HomingNpcAI.java:18-21
void HomingNpcAI::think() {
	// homings are not thinking to return :)
}

// Java HomingNpcAI.java:23-29
AttackIntention HomingNpcAI::chooseAttackIntention() {
	if (getTarget() && chooseSkillAttack(false))
		return AttackIntention::SKILL_ATTACK;

	return AttackIntention::SIMPLE_ATTACK;
}

// Java HomingNpcAI.java:31-37
bool HomingNpcAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai
