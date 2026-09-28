#include "aion/gameserver/network/aion/clientpackets/CM_REMOVE_ALTERED_STATE.h"

#include <string>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillSubType.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Creature;
using model::gameobjects::player::Player;
using skillengine::model::Effect;
using skillengine::model::SkillSubType;

CM_REMOVE_ALTERED_STATE::CM_REMOVE_ALTERED_STATE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_REMOVE_ALTERED_STATE.java:23-28
void CM_REMOVE_ALTERED_STATE::readImpl() {
	skillId = readUH();
	readC();
	readC(); // seen 1 with skillId 3573
}

// Java CM_REMOVE_ALTERED_STATE.java:30-42
void CM_REMOVE_ALTERED_STATE::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	runtime::Ptr<Effect> effect = player->getEffectController()->findBySkillId(skillId);
	if (effect) {
		if (effect->getSkillSubType() == SkillSubType::DEBUFF) {
			// Java `" (effector: " + effect.getEffector() + ")"`: String.valueOf, so a null effector would read "null"
			runtime::Ptr<Creature> effector = effect->getEffector();
			utils::audit::AuditLogger::log(*player, "tried to remove a debuff: " + std::to_string(skillId) + " " + effect->getSkillName() +
				" (effector: " + (effector ? effector->toString() : std::string("null")) + ")");
		} else {
			effect->endEffect();
		}
	}
}

AION_CLIENT_PACKET(CM_REMOVE_ALTERED_STATE);

} // namespace aion::gameserver::network::aion::clientpackets
