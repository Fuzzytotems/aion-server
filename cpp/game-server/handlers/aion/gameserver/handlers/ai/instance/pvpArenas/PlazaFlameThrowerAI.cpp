#include "aion/gameserver/handlers/ai/instance/pvpArenas/PlazaFlameThrowerAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::pvpArenas {

AION_AI(PlazaFlameThrowerAI, "plaza_flame_thrower");

// Java PlazaFlameThrowerAI.java:27-34
void PlazaFlameThrowerAI::handleDialogStart(Player& player) {
	runtime::Ptr<InstanceScore> instance = getPosition()->getWorldMapInstance()->getInstanceHandler()->getInstanceScore();
	if (instance != nullptr && !instance->isStartProgress()) {
		return;
	}
	ShifterAI::handleDialogStart(player);
}

// Java PlazaFlameThrowerAI.java:36-63
void PlazaFlameThrowerAI::handleUseItemFinish(Player& player) {
	ShifterAI::handleUseItemFinish(player);
	if (!isRewarded.get()) {
		isRewarded = true;
		AIActions::handleUseItemFinish(*this, player);
		switch (getNpcId()) {
			case 701169:
				useSkill(getNpcs(701178));
				useSkill(getNpcs(701192));
				break;
			case 701170:
				useSkill(getNpcs(701177));
				useSkill(getNpcs(701191));
				break;
			case 701171:
				useSkill(getNpcs(701176));
				useSkill(getNpcs(701190));
				break;
			case 701172:
				useSkill(getNpcs(701175));
				useSkill(getNpcs(701189));
				break;
		}
		AIActions::scheduleRespawn(*this);
		AIActions::deleteOwner(*this);
	}
}

// Java PlazaFlameThrowerAI.java:65-71 (Java's cast to PvPArenaScore: a ClassCastException for another score)
void PlazaFlameThrowerAI::useSkill(const std::vector<runtime::Ptr<Npc>>& npcs) {
	runtime::Ptr<PvPArenaScore> instance =
		runtime::cast<PvPArenaScore>(getPosition()->getWorldMapInstance()->getInstanceHandler()->getInstanceScore());
	for (const runtime::Ptr<Npc>& npc : npcs) {
		int32_t skill = instance->getNpcBonusSkill(npc->getNpcId());
		SkillEngine::getInstance().getSkill(*npc, skill >> 8, skill & 0xFF, runtime::Ptr<VisibleObject>(npc))->useNoAnimationSkill();
	}
}

// Java PlazaFlameThrowerAI.java:73-75
std::vector<runtime::Ptr<Npc>> PlazaFlameThrowerAI::getNpcs(int32_t npcId) {
	return getPosition()->getWorldMapInstance()->getNpcs({npcId});
}

} // namespace aion::gameserver::handlers::ai::instance::pvpArenas
