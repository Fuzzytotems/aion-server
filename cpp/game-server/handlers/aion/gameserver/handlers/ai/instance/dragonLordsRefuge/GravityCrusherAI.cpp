#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/GravityCrusherAI.h"

#include <optional>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(GravityCrusherAI, "gravity_crusher");

// Java GravityCrusherAI.java:29-41
void GravityCrusherAI::handleSpawned() {
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
		transform();
	}, 2000);
}

// Java GravityCrusherAI.java:43-48
void GravityCrusherAI::transform() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead())
			getOwner().queueSkill(getNpcId() == 283141 ? 20967 : 21900, 1);
	}, 30000);
}

// Java GravityCrusherAI.java:50-60
void GravityCrusherAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 20967:
		case 21900:
			spawn(getOwner().getLevel() == 65 ? getNpcId() + 1 : getNpcId() - 1, getOwner().getX(), getOwner().getY(), getOwner().getZ(),
				getOwner().getHeading());
			AIActions::deleteOwner(*this);
			break;
	}
}

// Java GravityCrusherAI.java:62-66
void GravityCrusherAI::handleMoveArrived() {
	AggressiveNpcAI::handleMoveArrived();
	getOwner().queueSkill(20987, 1);
}

// Java GravityCrusherAI.java:68-74
bool GravityCrusherAI::ask(AIQuestion question) {
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
