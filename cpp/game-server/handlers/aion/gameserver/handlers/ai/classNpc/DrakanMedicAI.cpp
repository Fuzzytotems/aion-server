#include "aion/gameserver/handlers/ai/classNpc/DrakanMedicAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"

namespace aion::gameserver::handlers::ai::classNpc {

AION_AI(DrakanMedicAI, "drakanmedic");

// Java DrakanMedicAI.java:19-25
void DrakanMedicAI::handleAttack(runtime::Ptr<Creature> creature) {
	DrakanPriestAI::handleAttack(creature);
	if (commons::utils::Rnd::chance() < 3) {
		spawnServants(getOwner().getObjectTemplate()->getRating() == model::templates::npc::NpcRating::NORMAL ? 281621 : 281839, 1);
	}
}

} // namespace aion::gameserver::handlers::ai::classNpc
