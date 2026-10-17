#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/SpectralWarriorAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/instance/StageType.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(SpectralWarriorAI, "spectral_warrior");

// Java SpectralWarriorAI.java:24-28
void SpectralWarriorAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java SpectralWarriorAI.java:30-34
void SpectralWarriorAI::handleHpPhase(int32_t phaseHpPercent) {
	getPosition()->getWorldMapInstance()->getInstanceHandler()->onChangeStage(model::instance::StageType::START_STAGE_6_ROUND_5);
	ThreadPoolManager::getInstance().schedule({this}, [this] { resurrectAllies(); }, 2000);
}

// Java SpectralWarriorAI.java:36-52
void SpectralWarriorAI::resurrectAllies() {
	getKnownList().forEachNpc([this](Npc& npc) {
		if (npc.isDead())
			return;
		switch (npc.getNpcId()) {
			case 205413:
				spawn(217576, npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
				npc.getController().delete_();
				break;
			case 205414:
				spawn(217577, npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
				npc.getController().delete_();
				break;
		}
	});
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
