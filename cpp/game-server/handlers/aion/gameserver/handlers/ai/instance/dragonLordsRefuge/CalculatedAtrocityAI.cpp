#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/CalculatedAtrocityAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(CalculatedAtrocityAI, "calculated_atrocity");

// Java CalculatedAtrocityAI.java:30-33
bool CalculatedAtrocityAI::canThink() {
	return false;
}

// Java CalculatedAtrocityAI.java:35-38
ItemAttackType CalculatedAtrocityAI::modifyAttackType(ItemAttackType type) {
	return ItemAttackType::MAGICAL_FIRE;
}

// Java CalculatedAtrocityAI.java:40-43
float CalculatedAtrocityAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return 0;
}

// Java CalculatedAtrocityAI.java:45-48
float CalculatedAtrocityAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	return damage * 0.7f;
}

// Java CalculatedAtrocityAI.java:50-57
void CalculatedAtrocityAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();

	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { calculateAndApplyDamage(); }, 500, 2000));

	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 11000);
}

// Java CalculatedAtrocityAI.java:59-64
void CalculatedAtrocityAI::calculateAndApplyDamage() {
	getKnownList().forEachPlayer([this](Player& p) {
		if (getOwner().canSee(runtime::Ptr<VisibleObject>(p)) && PositionUtil::isInRange(getOwner(), p, 45) &&
			PositionUtil::isInFrontOf(p, getOwner(), 45))
			SkillEngine::getInstance().applyEffectDirectly(21894, getOwner(), p);
	});
}

// Java CalculatedAtrocityAI.java:66-70
void CalculatedAtrocityAI::handleDespawned() {
	task.get()->cancel(true);
	GeneralNpcAI::handleDespawned();
}

// Java CalculatedAtrocityAI.java:72-78
bool CalculatedAtrocityAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
