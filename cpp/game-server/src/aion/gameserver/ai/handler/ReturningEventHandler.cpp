#include "aion/gameserver/ai/handler/ReturningEventHandler.h"

#include <vector>

#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/DispelSlotType.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::ai::handler {

using event::AIEventType;
using manager::EmoteManager;
using manager::WalkManager;
using model::gameobjects::Npc;
using model::skill::NpcSkillEntry;

void ReturningEventHandler::onNotAtHome(NpcAI& npcAI) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "onNotAtHome");
	}
	if (!npcAI.isMoveSupported()) {
		npcAI.onGeneralEvent(AIEventType::BACK_HOME);
	} else if (npcAI.setStateIfNot(AIState::RETURNING)) {
		npcAI.setSubStateIfNot(AISubState::NONE);
		if (npcAI.isLogging()) {
			AILogger::info(npcAI, "returning and restoring");
		}
		Npc& npc = npcAI.getOwner();
		EmoteManager::emoteStartReturning(npc);
		if (npc.isPathWalker() && WalkManager::startWalking(npcAI))
			return;
		npc.getMoveController()->returnToLastStepOrSpawn();
	}
}

void ReturningEventHandler::onBackHome(NpcAI& npcAI) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "onBackHome");
	}
	npcAI.getOwner().getMoveController()->clearBackSteps();
	if (npcAI.setStateIfNot(AIState::IDLE)) {
		npcAI.setSubStateIfNot(AISubState::NONE);
		npcAI.getOwner().getEffectController()->removeByDispelSlotType(skillengine::model::DispelSlotType::BUFF);
		EmoteManager::emoteStartIdling(npcAI.getOwner());
		npcAI.think();
		Npc& npc = npcAI.getOwner();
		std::vector<runtime::Ptr<NpcSkillEntry>> skills = npc.getSkillList()->getPostSpawnSkills();
		if (!skills.empty()) {
			for (const runtime::Ptr<NpcSkillEntry>& s : skills)
				skillengine::SkillEngine::getInstance()
					.getSkill(npc, s->getSkillId(), s->getSkillLevel(), runtime::Ptr<model::gameobjects::VisibleObject>(npc))
					->useWithoutPropSkill();
		}
	}
	npcAI.getOwner().getPosition()->getWorldMapInstance()->getInstanceHandler()->onBackHome(npcAI.getOwner());
}

} // namespace aion::gameserver::ai::handler
