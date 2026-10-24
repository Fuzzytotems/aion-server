#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/UltimateAtrocityAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(UltimateAtrocityAI, "ultimate_atrocity");

// Java UltimateAtrocityAI.java:28-31
ItemAttackType UltimateAtrocityAI::modifyAttackType(ItemAttackType type) {
	return ItemAttackType::MAGICAL_FIRE;
}

// Java UltimateAtrocityAI.java:33-36
float UltimateAtrocityAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	return damage / 4;
}

// Java UltimateAtrocityAI.java:38-54
void UltimateAtrocityAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	int32_t skill;
	switch (getNpcId()) {
		case 283244:
			skill = 21160;
			break;
		case 283240:
			skill = 21156;
			break;
		case 283237:
		case 283241:
			skill = 20923;
			break;
		default:
			skill = 0;
			break;
	}

	if (skill == 0)
		return;

	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, skill] { AIActions::useSkill(*this, skill); }, 500, 2000));

	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 11000);
}

// Java UltimateAtrocityAI.java:56-60
void UltimateAtrocityAI::handleDespawned() {
	task.get()->cancel(true); // Java: task.cancel(true) (null without a skill: NPE)
	GeneralNpcAI::handleDespawned();
}

// Java UltimateAtrocityAI.java:62-68
bool UltimateAtrocityAI::ask(AIQuestion question) {
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
