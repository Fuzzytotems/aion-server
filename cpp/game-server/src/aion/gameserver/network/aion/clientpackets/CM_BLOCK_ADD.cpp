#include "aion/gameserver/network/aion/clientpackets/CM_BLOCK_ADD.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_RESPONSE.h"
#include "aion/gameserver/services/SocialService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/Util.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BLOCK_ADD::CM_BLOCK_ADD(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BLOCK_ADD.java:28-31
void CM_BLOCK_ADD::readImpl() {
	targetName = readS();
	reason = readS();
}

// Java CM_BLOCK_ADD.java:34-50
void CM_BLOCK_ADD::runImpl() {
	using serverpackets::SM_BLOCK_RESPONSE;
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	const runtime::Ptr<model::gameobjects::player::PlayerCommonData> target =
		services::player::PlayerService::getOrLoadPlayerCommonData(utils::Util::convertName(targetName));
	if (commons::utils::StringUtils::equalsIgnoreCase(player->getName(), targetName))
		sendPacket(SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::CANT_BLOCK_SELF, targetName));
	else if (player->getBlockList()->isFull())
		sendPacket(SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::LIST_FULL, targetName));
	else if (target == nullptr)
		sendPacket(SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::TARGET_NOT_FOUND, targetName));
	else if (player->getFriendList().getFriend(target->getPlayerObjId()) != nullptr)
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_BLOCKLIST_NO_BUDDY());
	else if (player->getBlockList()->contains(target->getPlayerObjId()))
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_BLOCKLIST_ALREADY_BLOCKED());
	else
		services::SocialService::addBlockedUser(*player, *target, reason);
}

AION_CLIENT_PACKET(CM_BLOCK_ADD);

} // namespace aion::gameserver::network::aion::clientpackets
