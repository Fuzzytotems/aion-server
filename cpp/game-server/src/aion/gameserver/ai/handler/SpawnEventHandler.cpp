#include "aion/gameserver/ai/handler/SpawnEventHandler.h"

#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::ai::handler {

using model::gameobjects::Npc;
using model::skill::NpcSkillEntry;

void SpawnEventHandler::onSpawn(NpcAI& npcAI) {
	if (npcAI.setStateIfNot(AIState::IDLE)) {
		npcAI.think();
		Npc& npc = npcAI.getOwner();
		// NpcSkillList::getPostSpawnSkills answers Java's filter since M5b-2 part 3 (m5b2-plan.md D7, D11), so the 199 post-spawn entries are cast
		// here, synchronously and without a catch, as in Java
		std::vector<runtime::Ptr<NpcSkillEntry>> skills = npc.getSkillList()->getPostSpawnSkills();
		if (!skills.empty()) {
			for (const runtime::Ptr<NpcSkillEntry>& s : skills)
				skillengine::SkillEngine::getInstance()
					.getSkill(npc, s->getSkillId(), s->getSkillLevel(), runtime::Ptr<model::gameobjects::VisibleObject>(npc))
					->useWithoutPropSkill();
		}
	}
}

void SpawnEventHandler::onDespawn(NpcAI& npcAI) {
	npcAI.setStateIfNot(AIState::DESPAWNED);
}

void SpawnEventHandler::onBeforeSpawn(NpcAI& npcAI) {
}

} // namespace aion::gameserver::ai::handler
