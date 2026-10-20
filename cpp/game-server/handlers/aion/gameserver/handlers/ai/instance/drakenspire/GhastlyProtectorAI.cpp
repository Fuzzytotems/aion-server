#include "aion/gameserver/handlers/ai/instance/drakenspire/GhastlyProtectorAI.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(GhastlyProtectorAI, "drakenspire_ghastly_protector");

// Java GhastlyProtectorAI.java:23-26
ItemAttackType GhastlyProtectorAI::modifyAttackType(ItemAttackType type) {
	return ItemAttackType::MAGICAL_WIND;
}

// Java GhastlyProtectorAI.java:28-33
void GhastlyProtectorAI::handleSpawned() {
	AggressiveNoLootNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { aggroPlayer(); }, 1000);
	getOwner().getGameStats()->setNextSkillDelay(0);
}

// Java GhastlyProtectorAI.java:35-40
void GhastlyProtectorAI::aggroPlayer() {
	for (const runtime::Ptr<Player>& p : getKnownList().streamPlayers()) {
		if (!p->isDead() && PositionUtil::isInRange(*p, 152.38f, 518.68f, 1749.6f, 24)) { // findAny
			getAggroList().addHate(*p, 10000);
			break;
		}
	}
}

// Java GhastlyProtectorAI.java:42-46
void GhastlyProtectorAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21883)
		addHateToRandomTarget();
}

// Java GhastlyProtectorAI.java:48-50
void GhastlyProtectorAI::addHateToRandomTarget() {
	// Java: addHate(null, ...) is a no-op (AggroList.isAware(null) is false)
	if (runtime::Ptr<Creature> target = getAggroList().getTarget(AggroTarget::RANDOM); target != nullptr)
		getAggroList().addHate(*target, 10000);
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
