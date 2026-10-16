#include "aion/gameserver/services/player/PlayerChatService.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::services::player {

static const auto playerLog = commons::logging::LoggerFactory::getLogger("CHAT_LOG");
static const auto gmLog = commons::logging::LoggerFactory::getLogger("ADMINAUDIT_LOG");

using model::ChatType;

// Java PlayerChatService.java:19-26
bool PlayerChatService::isFlooding(model::gameobjects::player::Player& player) {
	player.setLastMessageTime();

	if (player.floodMsgCount() > configs::main::SecurityConfig::FLOOD_MSG.load())
		return true;

	return false;
}

// Java PlayerChatService.java:28-30
void PlayerChatService::logWhisper(model::gameobjects::player::Player& sender, model::gameobjects::player::Player& receiver,
	std::string_view message) {
	logMessage(sender, ChatType::WHISPER, message, runtime::Ptr<model::gameobjects::player::Player>(receiver));
}

// Java PlayerChatService.java:32-34
void PlayerChatService::logMessage(model::gameobjects::player::Player& sender, model::ChatType type, std::string_view message) {
	logMessage(sender, type, message, nullptr);
}

// Java PlayerChatService.java:36-74
void PlayerChatService::logMessage(model::gameobjects::player::Player& sender, model::ChatType type, std::string_view message,
	runtime::Ptr<model::gameobjects::player::Player> receiver) {
	const auto* log = &playerLog;

	// log whisper to adminaudit.log, if GM is involved (ignores private chat logging settings)
	if (type == ChatType::WHISPER && (sender.isStaff() || (receiver && receiver->isStaff())) && configs::main::LoggingConfig::LOG_GMAUDIT.load()) {
		log = &gmLog;
	} else {
		switch (type) {
			case ChatType::WHISPER:
			case ChatType::LEGION:
				if (!configs::main::LoggingConfig::LOG_PRIVATE_CHATS.load())
					return;
				break;
			default:
				if (!configs::main::LoggingConfig::LOG_GENERAL_CHATS.load())
					return;
		}
	}

	// Java type.toString(): the constant's name
	const std::string typeName(xml::enumName(type));
	switch (type) {
		case ChatType::WHISPER:
			log->info("[" + typeName + "] - [" + sender.getName() + "]>[" + (receiver ? receiver->getName() : std::string()) + "]: " +
				std::string(message));
			break;
		case ChatType::GROUP:
		case ChatType::ALLIANCE:
		case ChatType::GROUP_LEADER:
		case ChatType::LEAGUE:
		case ChatType::LEAGUE_ALERT:
			log->info("[" + typeName + "] <" + std::to_string(sender.getCurrentTeamId()) + "> - [" + sender.getName() + "]: " + std::string(message));
			break;
		case ChatType::LEGION:
			// Java: sender.getLegion().getName() - a NullPointerException for a player without a legion. Since the owner's correction of
			// 2026-10-05 CM_CHAT_MESSAGE_PUBLIC checks isLegionMember before it logs, so its LEGION chat never gets here without a legion
			// (docs/deviations/P5-08.md, "Chat"); another caller would still get Java's exception
			if (runtime::Ptr<model::team::legion::Legion> legion = sender.getLegion())
				log->info("[" + typeName + "] <" + legion->getName() + "> - [" + sender.getName() + "]: " + std::string(message));
			else
				throw runtime::NullPointerException("Player.getLegion() of " + sender.getName());
			break;
		case ChatType::NORMAL:
		case ChatType::SHOUT:
		default:
			log->info("[" + typeName + "] - [" + sender.getName() + "](" + std::string(xml::enumName(sender.getRace())) + "): " + std::string(message));
			break;
	}
}

} // namespace aion::gameserver::services::player
