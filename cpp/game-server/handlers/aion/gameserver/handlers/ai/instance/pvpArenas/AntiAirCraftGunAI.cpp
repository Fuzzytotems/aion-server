#include "aion/gameserver/handlers/ai/instance/pvpArenas/AntiAirCraftGunAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::pvpArenas {

AION_AI(AntiAirCraftGunAI, "antiaircraftgun");

// Java AntiAirCraftGunAI.java:23-30
void AntiAirCraftGunAI::handleDialogStart(Player& player) {
	runtime::Ptr<InstanceScore> instance = getPosition()->getWorldMapInstance()->getInstanceHandler()->getInstanceScore();
	if (instance != nullptr && !instance->isStartProgress()) {
		return;
	}
	ActionItemNpcAI::handleDialogStart(player);
}

// Java AntiAirCraftGunAI.java:32-54 (the local owner is ownerNpc: it hides the AI's member)
void AntiAirCraftGunAI::handleUseItemFinish(Player& player) {
	Npc& ownerNpc = getOwner();
	TeleportService::teleportTo(player, ownerNpc.getWorldId(), ownerNpc.getInstanceId(), ownerNpc.getX(), ownerNpc.getY(), ownerNpc.getZ(),
		ownerNpc.getHeading());
	int32_t morphSkill = 0;
	switch (getNpcId()) {
		case 701185: // 46 lvl morph 218803
		case 701321:
			morphSkill = 0x4E502E; // 20048 46
			break;
		case 701199: // 51 lvl morph 218804
		case 701322:
			morphSkill = 0x4E5133; // 20049 51
			break;
		case 701213: // 56 lvl morph 218805
		case 701323:
			morphSkill = 0x4E5238; // 20050 56
			break;
	}
	SkillEngine::getInstance().getSkill(getOwner(), morphSkill >> 8, morphSkill & 0xFF, runtime::Ptr<VisibleObject>(player))->useNoAnimationSkill();
	AIActions::scheduleRespawn(*this);
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::pvpArenas
