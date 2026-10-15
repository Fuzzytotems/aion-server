#include "aion/gameserver/handlers/ai/instance/theHexway/CaptainJarkaAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::theHexway {

AION_AI(CaptainJarkaAI, "captain_jarka");

// Java CaptainJarkaAI.java:21-33 (Java's cast of the smoke to Npc: 282465 is an npc)
void CaptainJarkaAI::handleDied() {
	SummonerAI::handleDied();
	// spawn smoke on death
	runtime::Ptr<WorldPosition> currentPos = getPosition();
	runtime::Ptr<Npc> smoke = runtime::cast<Npc>(spawn(282465, currentPos->getX(), currentPos->getY(), currentPos->getZ(), int8_t{0}));
	smoke->getController().delete_();
	// delete all npcs in range
	getKnownList().forEachNpc([this](Npc& npc) {
		if (PositionUtil::isInRange(getOwner(), npc, 15))
			npc.getController().delete_();
	});
}

} // namespace aion::gameserver::handlers::ai::instance::theHexway
