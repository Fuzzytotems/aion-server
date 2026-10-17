#include "aion/gameserver/handlers/ai/instance/drakenspire/OrissanSummonHelperAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(OrissanSummonHelperAI, "orissan_summon_helper");

// Java OrissanSummonHelperAI.java:25-28
float OrissanSummonHelperAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return 0;
}

// Java OrissanSummonHelperAI.java:30-34
void OrissanSummonHelperAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	handleSkillTask();
}

// Java OrissanSummonHelperAI.java:36-41
void OrissanSummonHelperAI::handleSkillTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		AIActions::targetSelf(*this);
		AIActions::useSkill(*this, 21647, 67);
	}, 1000);
}

// Java OrissanSummonHelperAI.java:43-50
void OrissanSummonHelperAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		runtime::Ptr<WorldPosition> p = getPosition();
		spawn(getNpcId() == 855607 ? 855699 : 855700, p->getX(), p->getY(), p->getZ(), p->getHeading());
		AIActions::deleteOwner(*this);
	}, 1500);
}

// Java OrissanSummonHelperAI.java:52-54
void OrissanSummonHelperAI::handleBackHome() {
}

// Java OrissanSummonHelperAI.java:56-61
bool OrissanSummonHelperAI::ask(AIQuestion question) {
	if (question == AIQuestion::REWARD_AP_XP_DP_LOOT)
		return false;
	return GeneralNpcAI::ask(question);
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
