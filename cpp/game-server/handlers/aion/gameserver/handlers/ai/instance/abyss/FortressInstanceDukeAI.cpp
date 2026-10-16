#include "aion/gameserver/handlers/ai/instance/abyss/FortressInstanceDukeAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::abyss {

AION_AI(FortressInstanceDukeAI, "fortress_instance_duke");

// Java FortressInstanceDukeAI.java:23-26
void FortressInstanceDukeAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t /*skillLevel*/) {
	if (skillTemplate->getSkillId() == 18003)
		spawn(284978, getOwner().getX(), getOwner().getY(), getOwner().getZ(), getOwner().getHeading());
}

// Java FortressInstanceDukeAI.java:28-33
void FortressInstanceDukeAI::deleteSummons() {
	getPosition()->getWorldMapInstance()->forEachNpc([](Npc& npc) {
		if (npc.getNpcId() >= 284978 && npc.getNpcId() <= 284981)
			npc.getController().delete_();
	});
}

// Java FortressInstanceDukeAI.java:36-39
void FortressInstanceDukeAI::handleDied() {
	AggressiveNpcAI::handleDied();
	deleteSummons();
}

// Java FortressInstanceDukeAI.java:42-45
void FortressInstanceDukeAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	deleteSummons();
}

} // namespace aion::gameserver::handlers::ai::instance::abyss
