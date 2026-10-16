#include "aion/gameserver/handlers/ai/instance/nightmareCircus/NightmareLordHeiramuneCloneAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(NightmareLordHeiramuneCloneAI, "nightmarelordheiramuneclone");

// Java NightmareLordHeiramuneCloneAI.java:24-28
void NightmareLordHeiramuneCloneAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	heal();
}

// Java NightmareLordHeiramuneCloneAI.java:30-39
void NightmareLordHeiramuneCloneAI::heal() {
	runtime::FutureRef task = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(233467);
		if (boss != nullptr && !boss->isDead()) {
			SkillEngine::getInstance().getSkill(getOwner(), 21342, 1, boss)->useSkill();
		} else
			AIActions::deleteOwner(*this);
	}, 5000, 10000);
	getOwner().getController().addTask(model::TaskId::SKILL_USE, task);
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
