#include "aion/gameserver/network/aion/clientpackets/CM_FRIEND_STATUS.h"

#include <cstdint>
#include <optional>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/FriendList_StatusInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_STATUS.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

// Java: a per-instance `private final Logger log` with the class as its name
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_FRIEND_STATUS");

using model::gameobjects::player::FriendList;

CM_FRIEND_STATUS::CM_FRIEND_STATUS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_FRIEND_STATUS::readImpl() {
	status = readC();
}

void CM_FRIEND_STATUS::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	// Java: Status.getByValue(status), null for an unknown id
	std::optional<FriendList::Status> statusEnum = model::gameobjects::player::getByValue(status);
	if (!statusEnum) {
		log.warn("received unknown status id {}", static_cast<int32_t>(status));
		statusEnum = FriendList::Status::ONLINE;
	}
	activePlayer->getFriendList().setStatus(*statusEnum, *activePlayer->getCommonData());
	sendPacket(serverpackets::SM_FRIEND_STATUS(status));
}

AION_CLIENT_PACKET(CM_FRIEND_STATUS);

} // namespace aion::gameserver::network::aion::clientpackets
