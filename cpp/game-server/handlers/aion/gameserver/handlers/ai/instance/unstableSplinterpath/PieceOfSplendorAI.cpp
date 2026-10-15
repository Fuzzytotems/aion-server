#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/PieceOfSplendorAI.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(PieceOfSplendorAI, "pieceofsplendor");

// Java PieceOfSplendorAI.java:21-24
void PieceOfSplendorAI::handleCreatureSee(Creature& creature) {
	checkDistance(creature);
}

// Java PieceOfSplendorAI.java:26-29
void PieceOfSplendorAI::handleCreatureMoved(Creature& creature) {
	checkDistance(creature);
}

// Java PieceOfSplendorAI.java:31-41
void PieceOfSplendorAI::checkDistance(Creature& creature) {
	if (runtime::as<Npc>(runtime::Ptr<Creature>(creature)) != nullptr) {
		runtime::Ptr<Npc> ebonsoul = getPosition()->getWorldMapInstance()->getNpc(219552);
		if (ebonsoul != nullptr && PositionUtil::isInRange(getOwner(), *ebonsoul, 5) && ebonsoul->getEffectController()->hasAbnormalEffect(19159)) {
			ebonsoul->getEffectController()->removeEffect(19159);
			runtime::Ptr<Npc> rukril = getPosition()->getWorldMapInstance()->getNpc(219551);
			if (rukril != nullptr && rukril->getEffectController()->hasAbnormalEffect(19266))
				rukril->getEffectController()->removeEffect(19266);
		}
	}
}

// Java PieceOfSplendorAI.java:43-49
bool PieceOfSplendorAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
