#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/IDTiamat_2_DrakanNpcAI.h"

#include <optional>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(IDTiamat_2_DrakanNpcAI, "IDTiamat_2_Drakan_NPC");

// Java IDTiamat_2_DrakanNpcAI.java:27-30
float IDTiamat_2_DrakanNpcAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return damage;
}

// Java IDTiamat_2_DrakanNpcAI.java:32-40
void IDTiamat_2_DrakanNpcAI::handleSpawned() {
	OneDmgAI::handleSpawned();
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	// Java: AIActions.targetCreature(this, Rnd.get(players)), which sets a null target when nobody is inside
	if (std::optional<runtime::Ptr<Player>> target = commons::utils::Rnd::get(instance->getPlayersInside()); target.has_value())
		AIActions::targetCreature(*this, **target);
	else
		getOwner().setTarget(nullptr);
	setStateIfNot(gameserver::ai::AIState::WALKING);
	getOwner().setState(CreatureState::ACTIVE, true);
	getMoveController()->moveToTargetObject();
	PacketSendUtility::broadcastToMap(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::WALK));
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
