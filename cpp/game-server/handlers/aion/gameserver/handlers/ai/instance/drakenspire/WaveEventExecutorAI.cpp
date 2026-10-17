#include "aion/gameserver/handlers/ai/instance/drakenspire/WaveEventExecutorAI.h"

#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(WaveEventExecutorAI, "wave_event_executor");

// Java WaveEventExecutorAI.java:25-33
void WaveEventExecutorAI::handleMoveArrived() {
	GeneralNpcAI::handleMoveArrived();
	if (getOwner().getMoveController()->isStop() && isDestinationReached.compareAndSet(false, true)) {
		getOwner().getSpawn()->setWalkerId(std::string_view(""));
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501318);
		scheduleSummon();
	}
}

// Java WaveEventExecutorAI.java:35-40
void WaveEventExecutorAI::scheduleSummon() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		SkillEngine::getInstance().getSkill(getOwner(), 20839, 1, runtime::Ptr<VisibleObject>(getOwner()))->useSkill();
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501317, 2000);
	}, 3500);
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
