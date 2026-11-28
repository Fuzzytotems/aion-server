#include "aion/gameserver/utils/audit/AuditLogger.h"

#include <format>
#include <string>

#include "aion/commons/logging/Logger.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AutoBan.h"
#include "aion/gameserver/utils/audit/GMService.h"

namespace aion::gameserver::utils::audit {

namespace {
const commons::logging::Logger& auditLog() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("AUDIT_LOG"));
	return *logger;
}
} // namespace

/**
 * Replaces control characters so a client cannot forge log lines
 * <p>
 * C++: the UTF-8 bytes below 128 are exactly the ASCII characters (bytes of multibyte sequences are 128 or more), so a byte loop finds the
 * same characters as Java's loop over the UTF-16 chars.
 */
std::string AuditLogger::sanitize(std::string_view message) {
	std::string sb;
	sb.reserve(message.size());
	for (const char ch : message) {
		const unsigned char c = static_cast<unsigned char>(ch);
		if (c == '\n')
			sb += "\\n";
		else if (c == '\r')
			sb += "\\r";
		else if (ChatUtil::isInvalidCharacter(c))
			sb += std::format("\\u{:04x}", static_cast<int32_t>(c));
		else
			sb += ch;
	}
	return sb;
}

void AuditLogger::log(model::gameobjects::player::Player& player, std::string_view messageValue) {
	const std::string message = sanitize(messageValue);
	if (configs::main::PunishmentConfig::PUNISHMENT_ENABLE.load())
		AutoBan::punishment(player);
	if (configs::main::LoggingConfig::LOG_AUDIT.load())
		auditLog().info("{} {}", player.toString(), message);
	for (const runtime::Ptr<model::gameobjects::player::Player>& gm : GMService::getInstance().getOnlineStaffMembers()) {
		if (gm->hasAccess(configs::administration::AdminConfig::AUDIT_INFO.load()))
			PacketSendUtility::sendMessage(*gm, ChatUtil::charName(player) + " " + std::string(message), model::ChatType::YELLOW);
	}
}

void AuditLogger::log(runtime::Ptr<model::gameobjects::player::Player> player, std::string_view messageValue) {
	if (player) {
		log(*player, messageValue);
		return;
	}
	const std::string message = sanitize(messageValue);
	// the Java body with player == null
	if (configs::main::PunishmentConfig::PUNISHMENT_ENABLE.load())
		throw runtime::NullPointerException("AutoBan.punishment: player is null"); // player.getClientConnection()
	if (configs::main::LoggingConfig::LOG_AUDIT.load())
		auditLog().info("{} {}", "null", message); // Java: log.info("{} {}", player, message) with player null
	for (const runtime::Ptr<model::gameobjects::player::Player>& gm : GMService::getInstance().getOnlineStaffMembers()) {
		if (gm->hasAccess(configs::administration::AdminConfig::AUDIT_INFO.load()))
			throw runtime::NullPointerException("ChatUtil.charName: player is null"); // player.getName(true)
	}
}

} // namespace aion::gameserver::utils::audit
