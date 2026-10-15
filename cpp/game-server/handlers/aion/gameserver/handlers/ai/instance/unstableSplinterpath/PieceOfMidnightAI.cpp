#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/PieceOfMidnightAI.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(PieceOfMidnightAI, "pieceofmidnight");

// Java PieceOfMidnightAI.java:21-24
void PieceOfMidnightAI::handleCreatureSee(Creature& creature) {
	checkDistance(creature);
}

// Java PieceOfMidnightAI.java:26-29
void PieceOfMidnightAI::handleCreatureMoved(Creature& creature) {
	checkDistance(creature);
}

// Java PieceOfMidnightAI.java:31-41
void PieceOfMidnightAI::checkDistance(Creature& creature) {
	if (runtime::as<Npc>(runtime::Ptr<Creature>(creature)) != nullptr) {
		runtime::Ptr<Npc> rukril = getPosition()->getWorldMapInstance()->getNpc(219551);
		if (rukril != nullptr && PositionUtil::isInRange(getOwner(), *rukril, 5) && rukril->getEffectController()->hasAbnormalEffect(19266)) {
			rukril->getEffectController()->removeEffect(19266);
			runtime::Ptr<Npc> ebonsoul = getPosition()->getWorldMapInstance()->getNpc(219552);
			if (ebonsoul != nullptr && ebonsoul->getEffectController()->hasAbnormalEffect(19159))
				ebonsoul->getEffectController()->removeEffect(19159);
		}
	}
}

// Java PieceOfMidnightAI.java:43-49
bool PieceOfMidnightAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
