#include "aion/gameserver/utils/audit/AuditLogger.h"

#include <string>

#include "aion/commons/logging/Logger.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
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

void AuditLogger::log(model::gameobjects::player::Player& player, std::string_view message) {
	if (configs::main::PunishmentConfig::PUNISHMENT_ENABLE.load())
		AutoBan::punishment(player);
	if (configs::main::LoggingConfig::LOG_AUDIT.load())
		auditLog().info(player.toString() + " " + std::string(message));
	for (const runtime::Ptr<model::gameobjects::player::Player>& gm : GMService::getInstance().getOnlineStaffMembers()) {
		if (gm->hasAccess(configs::administration::AdminConfig::AUDIT_INFO.load()))
			PacketSendUtility::sendMessage(*gm, ChatUtil::charName(player) + " " + std::string(message), model::ChatType::YELLOW);
	}
}

} // namespace aion::gameserver::utils::audit
