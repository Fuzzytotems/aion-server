#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAssaultCommanderAI.h"

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionAssaultCommanderAI, "ahserion_assault_commander");

// Java AhserionAssaultCommanderAI.java:20-24 (no super call, as Java; a null template is Java's NullPointerException)
void AhserionAssaultCommanderAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	static_cast<void>(skillLevel);
	if (skillTemplate == nullptr)
		throw runtime::NullPointerException("AhserionAssaultCommanderAI: skill template");
	if (skillTemplate->getSkillId() == 20648) // Pincer Attack
		addHateToRndTarget();
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
