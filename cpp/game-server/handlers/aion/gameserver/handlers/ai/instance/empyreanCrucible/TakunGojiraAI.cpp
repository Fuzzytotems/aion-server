#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/TakunGojiraAI.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(TakunGojiraAI, "takun_gojira");

// Java TakunGojiraAI.java:21-33 (the anonymous Runnable is a lambda)
void TakunGojiraAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		counterpart = runtime::Ref<Npc>(getPosition()->getWorldMapInstance()->getNpc(getNpcId() == 217596 ? 217597 : 217596));
		runtime::Ptr<Npc> other = counterpart.get();
		if (other != nullptr)
			getAggroList().addHate(*other, 1000000);
	}, 500);
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
