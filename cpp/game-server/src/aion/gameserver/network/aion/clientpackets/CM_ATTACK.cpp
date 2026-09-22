#include "aion/gameserver/network/aion/clientpackets/CM_ATTACK.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** Logger */
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_ATTACK");

using model::gameobjects::Creature;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;

CM_ATTACK::CM_ATTACK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_ATTACK::readImpl() {
	targetObjectId = readD(); // empty
	attackno = readUC();      // empty
	time = readUH();          // empty
	type = readUC();          // empty
}

void CM_ATTACK::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player->isDead())
		return;

	if (player->isProtectionActive())
		player->getController().stopProtectionActiveTask();

	runtime::Ptr<VisibleObject> obj = player->getKnownList().getObject(targetObjectId);
	if (runtime::Ptr<Creature> creature = runtime::as<Creature>(obj)) {
		player->getController().attackTarget(creature, time, false);
	} else if (obj) {
		log.warn(player->toString() + " attacking unsupported target " + obj->toString());
	}
}

AION_CLIENT_PACKET(CM_ATTACK);

} // namespace aion::gameserver::network::aion::clientpackets
