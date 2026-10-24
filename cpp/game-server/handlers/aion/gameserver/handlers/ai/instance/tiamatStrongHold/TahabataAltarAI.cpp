#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/TahabataAltarAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(TahabataAltarAI, "tahabataaltar");

// Java TahabataAltarAI.java:22-25
void TahabataAltarAI::handleCreatureSee(Creature& creature) {
	checkDistance(creature);
}

// Java TahabataAltarAI.java:27-30
void TahabataAltarAI::handleCreatureMoved(Creature& creature) {
	checkDistance(creature);
}

// Java TahabataAltarAI.java:32-47. Correction of the Java code (owner's decision 2026-10-19, both branches; was J-I6-5): the bands are
// checked on the altars' real ids 283116 and 283118, where Java checked 283253 and 283255 (ids no npc template has)
void TahabataAltarAI::checkDistance(Creature& creature) {
	int32_t ownerId = getNpcId(); // Java: int owner (renamed: it hides AbstractAI::owner)
	int32_t debuff;
	switch (ownerId) {
		case 283116:
			debuff = 20970;
			break;
		case 283118:
			debuff = 20971;
			break;
		default:
			debuff = 0;
			break;
	}
	if (runtime::as<Player>(creature) != nullptr) { // Java: creature instanceof Player
		if ((getNpcId() == 283116 && PositionUtil::isInRangeLimited(getOwner(), creature, 25, 37))
			|| (getNpcId() == 283118 && PositionUtil::isInRangeLimited(getOwner(), creature, 20, 25))) {
			if (!creature.getEffectController()->hasAbnormalEffect(debuff)) {
				AIActions::useSkill(*this, debuff);
			}
		}
	}
}

// Java TahabataAltarAI.java:49-55
bool TahabataAltarAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
