#include "aion/gameserver/network/aion/clientpackets/CM_SUMMON_ATTACK.h"

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_SUMMON_ATTACK::CM_SUMMON_ATTACK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SUMMON_ATTACK.java:30-37
void CM_SUMMON_ATTACK::readImpl() {
	summonObjId = readD();
	targetObjId = readD();
	unk1 = readC();
	time = readUH();
	unk3 = readC();
}

// Java CM_SUMMON_ATTACK.java:39-53
void CM_SUMMON_ATTACK::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();

	const runtime::Ptr<model::gameobjects::Creature> summonOrMercenary = player->getSummonOrMercenary(summonObjId);
	if (!summonOrMercenary) // commonly due to lags when the pet dies
		return;

	const runtime::Ptr<model::gameobjects::VisibleObject> obj = summonOrMercenary->getKnownList().getObject(targetObjId); // may be null due to lags during movement
	if (const runtime::Ptr<model::gameobjects::Creature> creature = runtime::as<model::gameobjects::Creature>(obj))
		summonOrMercenary->getController().attackTarget(creature, time, false);
	else if (obj) // not a creature (attack should be client restricted)
		utils::audit::AuditLogger::log(*player, "tried to use summon attack on a wrong target: " + obj->toString());
}

AION_CLIENT_PACKET(CM_SUMMON_ATTACK);

} // namespace aion::gameserver::network::aion::clientpackets
