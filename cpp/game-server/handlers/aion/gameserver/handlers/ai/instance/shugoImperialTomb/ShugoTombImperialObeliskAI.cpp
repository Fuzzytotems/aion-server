#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombImperialObeliskAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

AION_AI(ShugoTombImperialObeliskAI, "shugo_tomb_imperial_obelisk");

// Java ShugoTombImperialObeliskAI.java:24-27
bool ShugoTombImperialObeliskAI::canThink() {
	return false;
}

// Java ShugoTombImperialObeliskAI.java:29-33
void ShugoTombImperialObeliskAI::handleAttack(runtime::Ptr<Creature> creature) {
	GeneralNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java ShugoTombImperialObeliskAI.java:35-41
void ShugoTombImperialObeliskAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 70:
			SkillEngine::getInstance().applyEffectDirectly(21098, getOwner(), getOwner());
			break;
		case 35:
			SkillEngine::getInstance().applyEffectDirectly(21099, getOwner(), getOwner());
			break;
		default:
			break;
	}
}

// Java ShugoTombImperialObeliskAI.java:43-47
void ShugoTombImperialObeliskAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	SkillEngine::getInstance().applyEffectDirectly(21097, getOwner(), getOwner());
}

// Java ShugoTombImperialObeliskAI.java:49-56
bool ShugoTombImperialObeliskAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
