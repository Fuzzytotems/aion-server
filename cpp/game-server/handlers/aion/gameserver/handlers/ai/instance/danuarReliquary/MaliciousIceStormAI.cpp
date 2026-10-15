#include "aion/gameserver/handlers/ai/instance/danuarReliquary/MaliciousIceStormAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

AION_AI(MaliciousIceStormAI, "malicious_ice_storm");

// Java MaliciousIceStormAI.java:22-26
void MaliciousIceStormAI::handleSpawned() {
	NpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(*this, 21180); }, 1000);
}

// Java MaliciousIceStormAI.java:28-31
float MaliciousIceStormAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	return damage * 0.5f;
}

// Java MaliciousIceStormAI.java:33-37
void MaliciousIceStormAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21180)
		getOwner().getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
