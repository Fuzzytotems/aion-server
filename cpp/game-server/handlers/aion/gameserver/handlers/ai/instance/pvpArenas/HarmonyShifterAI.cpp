#include "aion/gameserver/handlers/ai/instance/pvpArenas/HarmonyShifterAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/instance/instancescore/HarmonyArenaScore.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::pvpArenas {

AION_AI(HarmonyShifterAI, "harmony_shifter");

// Java HarmonyShifterAI.java:27-44
void HarmonyShifterAI::handleUseItemFinish(Player& player) {
	ShifterAI::handleUseItemFinish(player);
	if (isRewarded.compareAndSet(false, true)) {
		AIActions::handleUseItemFinish(*this, player);
		switch (getNpcId()) {
			case 207116:
				useSkill(getNpcs(207118));
				useSkill(getNpcs(207119));
				break;
			case 207099:
				useSkill(getNpcs(207100));
				break;
		}
		AIActions::scheduleRespawn(*this);
		AIActions::deleteOwner(*this);
	}
}

// Java HarmonyShifterAI.java:46-52 (Java's cast to HarmonyArenaScore: a ClassCastException for another score)
void HarmonyShifterAI::useSkill(const std::vector<runtime::Ptr<Npc>>& npcs) {
	runtime::Ptr<HarmonyArenaScore> instance =
		runtime::cast<HarmonyArenaScore>(getPosition()->getWorldMapInstance()->getInstanceHandler()->getInstanceScore());
	for (const runtime::Ptr<Npc>& npc : npcs) {
		int32_t skill = instance->getNpcBonusSkill(npc->getNpcId());
		SkillEngine::getInstance().getSkill(*npc, skill >> 8, skill & 0xFF, runtime::Ptr<VisibleObject>(npc))->useNoAnimationSkill();
	}
}

// Java HarmonyShifterAI.java:54-56
std::vector<runtime::Ptr<Npc>> HarmonyShifterAI::getNpcs(int32_t npcId) {
	return getPosition()->getWorldMapInstance()->getNpcs({npcId});
}

} // namespace aion::gameserver::handlers::ai::instance::pvpArenas
