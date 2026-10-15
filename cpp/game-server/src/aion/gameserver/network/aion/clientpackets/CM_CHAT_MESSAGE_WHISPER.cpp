#include "aion/gameserver/network/aion/clientpackets/CM_CHAT_MESSAGE_WHISPER.h"

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/services/NameRestrictionService.h"
#include "aion/gameserver/services/player/PlayerChatService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using configs::main::CustomConfig;
using model::gameobjects::player::CustomPlayerState;
using model::gameobjects::player::Player;
using serverpackets::SM_SYSTEM_MESSAGE;

CM_CHAT_MESSAGE_WHISPER::CM_CHAT_MESSAGE_WHISPER(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CHAT_MESSAGE_WHISPER.java:52-55
void CM_CHAT_MESSAGE_WHISPER::readImpl() {
	name = readS();
	message = readS();
}

// Java CM_CHAT_MESSAGE_WHISPER.java:58-76
void CM_CHAT_MESSAGE_WHISPER::runImpl() {
	const std::string realName = utils::ChatUtil::getRealCharName(name);
	const runtime::Ptr<Player> sender = getConnection()->getActivePlayer();
	const runtime::Ptr<Player> receiver = world::World::getInstance().getPlayer(realName);

	if (receiver == nullptr) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(realName));
	} else if (receiver->isInCustomState(CustomPlayerState::NO_WHISPERS_MODE) && !sender->isStaff()) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_WHISPER_REFUSE(receiver->getName(true)));
	} else if (sender->getLevel() < CustomConfig::LEVEL_TO_WHISPER.load() && !receiver->isStaff()) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_CANT_WHISPER_LEVEL(std::to_string(CustomConfig::LEVEL_TO_WHISPER.load())));
	} else if (receiver->getBlockList()->contains(sender->getObjectId())) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_YOU_EXCLUDED(receiver->getName()));
	} else if (sender->getRace() != receiver->getRace() && !CustomConfig::SPEAKING_BETWEEN_FACTIONS.load() && !sender->isStaff() &&
			   !receiver->isStaff()) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_CANT_WHISPER_OTHER_RACE());
	} else {
		if (!restrictions::PlayerRestrictions::canChat(sender))
			return;
		services::player::PlayerChatService::logWhisper(*sender, *receiver, message);
		utils::PacketSendUtility::sendPacket(*receiver,
			serverpackets::SM_MESSAGE(*sender, services::NameRestrictionService::filterMessage(message), model::ChatType::WHISPER));
	}
}

AION_CLIENT_PACKET(CM_CHAT_MESSAGE_WHISPER);

} // namespace aion::gameserver::network::aion::clientpackets
