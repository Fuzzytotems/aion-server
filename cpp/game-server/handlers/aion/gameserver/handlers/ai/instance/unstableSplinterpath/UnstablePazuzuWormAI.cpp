#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstablePazuzuWormAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(UnstablePazuzuWormAI, "unstablepazuzuworm");

// Java UnstablePazuzuWormAI.java:21-28
void UnstablePazuzuWormAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		runtime::Ptr<Npc> pazuzu = getPosition()->getWorldMapInstance()->getNpc(219554);
		if (pazuzu != nullptr)
			AIActions::targetCreature(*this, *pazuzu);
		else
			getOwner().setTarget(nullptr);
		AIActions::useSkill(*this, 19291);
	}, 3000);
}

// Java UnstablePazuzuWormAI.java:30-36
bool UnstablePazuzuWormAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
