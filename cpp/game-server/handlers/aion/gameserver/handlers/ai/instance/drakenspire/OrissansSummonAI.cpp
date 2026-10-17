#include "aion/gameserver/handlers/ai/instance/drakenspire/OrissansSummonAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(OrissansSummonAI, "orissans_summon");

// Java OrissansSummonAI.java:28-45
float OrissansSummonAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	if (&attacker != &getOwner() && dynamic_cast<Npc*>(&attacker) != nullptr) {
		if (isActive.compareAndSet(false, true)) {
			runtime::Ref<Creature> capturedAttacker(attacker);
			ThreadPoolManager::getInstance().schedule({this}, [this, capturedAttacker] {
				if (getNpcId() == 855699) {
					AIActions::targetSelf(*this);
					AIActions::useSkill(*this, 21638, 67);
				} else {
					AIActions::targetCreature(*this, *capturedAttacker);
					AIActions::useSkill(*this, 21639, 13);
				}
			}, 1000);
		}
		return 10000;
	}
	return damage;
}

// Java OrissansSummonAI.java:47-50
void OrissansSummonAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	getOwner().getController().die();
}

// Java OrissansSummonAI.java:52-54
void OrissansSummonAI::handleBackHome() {
}

// Java OrissansSummonAI.java:56-63
bool OrissansSummonAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
