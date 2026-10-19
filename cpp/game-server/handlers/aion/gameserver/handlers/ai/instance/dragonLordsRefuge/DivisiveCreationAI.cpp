#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/DivisiveCreationAI.h"

#include <optional>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(DivisiveCreationAI, "divisive_creation");

// Java DivisiveCreationAI.java:35-46
void DivisiveCreationAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	const runtime::Ref<WorldMapInstance> instance(*getPosition()->getWorldMapInstance());
	ThreadPoolManager::getInstance().schedule({this}, [this, instance] {
		// Java: AIActions.targetCreature(this, Rnd.get(players)), which sets a null target when nobody is inside
		if (std::optional<runtime::Ptr<Player>> target = commons::utils::Rnd::get(instance->getPlayersInside()); target.has_value())
			AIActions::targetCreature(*this, **target);
		else
			getOwner().setTarget(nullptr);
		setStateIfNot(gameserver::ai::AIState::WALKING);
		getOwner().setState(CreatureState::ACTIVE, true);
		getMoveController()->moveToTargetObject();
		PacketSendUtility::broadcastToMap(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::WALK));
	}, 5000);
}

// Java DivisiveCreationAI.java:48-62
float DivisiveCreationAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	if (effect != nullptr) {
		switch (effect->getSkillId()) {
			case 20986:
				damage *= 0.6f;
				break;
			case 21897:
			case 21898:
				damage *= 0.5f;
				break;
		}
	}
	return damage;
}

// Java DivisiveCreationAI.java:64-67
ItemAttackType DivisiveCreationAI::modifyAttackType(ItemAttackType type) {
	return ItemAttackType::MAGICAL_EARTH;
}

// Java DivisiveCreationAI.java:69-75
bool DivisiveCreationAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
