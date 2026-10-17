#include "aion/gameserver/handlers/ai/instance/drakenspire/OrissanDoorDestroyerAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(OrissanDoorDestroyerAI, "orissan_door_destroyer");

// Java OrissanDoorDestroyerAI.java:21-25
void OrissanDoorDestroyerAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	scheduleGateDestruction();
}

// Java OrissanDoorDestroyerAI.java:27-34
void OrissanDoorDestroyerAI::scheduleGateDestruction() {
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501313, 7000);
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({731580}))
			if (isInRange(*npc, 15))
				SkillEngine::getInstance().getSkill(*npc, 20840, 1, runtime::Ptr<VisibleObject>(npc))->useWithoutPropSkill();
	}, 14000);
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
