#include "aion/gameserver/handlers/ai/instance/drakenspire/DimensionalWaveAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(DimensionalWaveAI, "drakenspire_dimensional_wave");

// Java DimensionalWaveAI.java:22-27
void DimensionalWaveAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21620) {
		ThreadPoolManager::getInstance().schedule({this}, [this] { calculateAndApplyDamage(); }, 1200); // Aligns visual hit and damage
	}
}

// Java DimensionalWaveAI.java:29-49
void DimensionalWaveAI::calculateAndApplyDamage() {
	for (const runtime::Ptr<Player>& p : getKnownList().streamPlayers()) {
		if (!(!p->isDead() && isInRange(*p, 29)))
			continue;
		int32_t headingTowardsPlayer = PositionUtil::getHeadingTowards(getPosition()->getX(), getPosition()->getY(), p->getX(), p->getY());
		int32_t headingMax = getPosition()->getHeading(); // 30 or 90
		int32_t headingMin = headingMax - 60;
		if (headingMin < 0) {
			headingMin += 120;
		}

		bool isHit;
		if (headingMin <= headingMax) {
			isHit = headingTowardsPlayer >= headingMin && headingTowardsPlayer <= headingMax;
		} else {
			isHit = headingTowardsPlayer >= headingMin || headingTowardsPlayer <= headingMax;
		}

		if (isHit) {
			SkillEngine::getInstance().applyEffect(21874, getOwner(), *p);
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
