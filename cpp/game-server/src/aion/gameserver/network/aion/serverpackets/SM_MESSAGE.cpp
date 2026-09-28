#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/ChatTypeInfo.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"

namespace aion::gameserver::network::aion::serverpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.serverpackets.SM_MESSAGE");

SM_MESSAGE::SM_MESSAGE(model::gameobjects::player::Player& sender, std::string_view messageValue, model::ChatType chatTypeValue)
	: SM_MESSAGE(runtime::Ptr<model::gameobjects::Creature>(sender),
		  sender.getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::HIDE) ? 0 : sender.getObjectId(), sender.getName(true),
		  messageValue, chatTypeValue) {
}

SM_MESSAGE::SM_MESSAGE(model::gameobjects::Npc& sender, std::string_view messageValue, model::ChatType chatTypeValue)
	: SM_MESSAGE(runtime::Ptr<model::gameobjects::Creature>(sender), sender.getObjectId(), sender.getName(), messageValue, chatTypeValue) {
}

SM_MESSAGE::SM_MESSAGE(int32_t senderObjectIdValue, std::string_view senderNameValue, std::string_view messageValue, model::ChatType chatTypeValue)
	: SM_MESSAGE(nullptr, senderObjectIdValue, senderNameValue, messageValue, chatTypeValue) {
}

SM_MESSAGE::SM_MESSAGE(runtime::Ptr<model::gameobjects::Creature> sender, int32_t senderObjectIdValue, std::string_view senderNameValue,
	std::string_view messageValue, model::ChatType chatTypeValue)
	: AionServerPacket(opcodeOf<SM_MESSAGE>) {
	std::string text(messageValue);
	int32_t length = commons::utils::StringUtils::utf16Length(text); // Java String.length(): UTF-16 code units
	if (length > MESSAGE_SIZE_LIMIT) {
		log.warn("Exceeded maximum string size for packet SM_MESSAGE.\nSize: {}\nMessage: {}", length, text);
		if (length > MESSAGE_SIZE_HARDCAP)
			text = commons::utils::StringUtils::substring(text, 0, MESSAGE_SIZE_HARDCAP); // shorten message to avoid send log error
	}
	if (sender != nullptr) {
		runtime::Ptr<model::gameobjects::player::Player> player = runtime::as<model::gameobjects::player::Player>(sender);
		if (player != nullptr && !model::isSysMsg(chatTypeValue) && !configs::main::CustomConfig::SPEAKING_BETWEEN_FACTIONS.load() && !player->isStaff()) {
			senderRace = static_cast<int8_t>(model::getRaceId(player->getRace()) + 1);
		}
		x = sender->getX();
		y = sender->getY();
		z = sender->getZ();
	}
	senderObjectId = senderObjectIdValue;
	senderName = senderNameValue;
	message = std::move(text);
	chatType = chatTypeValue;
}

void SM_MESSAGE::writeImpl(AionConnection* con) {
	runtime::Ptr<model::gameobjects::player::Player> activePlayer = detail::requireConnection(con, "SM_MESSAGE").getActivePlayer();
	if (activePlayer == nullptr)
		return;
	writeC(model::getId(chatType));
	writeC(activePlayer->isStaff() ? 0 : senderRace);
	writeD(senderObjectId);
	writeS(senderName);
	writeS(message);
	if (chatType == model::ChatType::SHOUT) {
		writeF(x);
		writeF(y);
		writeF(z);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
