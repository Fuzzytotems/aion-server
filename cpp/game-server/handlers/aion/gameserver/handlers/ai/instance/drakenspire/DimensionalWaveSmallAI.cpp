#include "aion/gameserver/handlers/ai/instance/drakenspire/DimensionalWaveSmallAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(DimensionalWaveSmallAI, "drakenspire_dimensional_wave_small");

// Java DimensionalWaveSmallAI.java:21-26
void DimensionalWaveSmallAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21620) {
		ThreadPoolManager::getInstance().schedule({this}, [this] { calculateAndApplyDamage(); }, 1200); // Aligns visual hit and damage
	}
}

// Java DimensionalWaveSmallAI.java:28-31
void DimensionalWaveSmallAI::calculateAndApplyDamage() {
	for (const runtime::Ptr<Player>& p : getKnownList().streamPlayers())
		if (!p->isDead() && isInRange(*p, 22))
			SkillEngine::getInstance().applyEffect(21874, getOwner(), *p);
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
