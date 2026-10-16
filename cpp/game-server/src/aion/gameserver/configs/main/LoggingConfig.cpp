#include "aion/gameserver/configs/main/LoggingConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void LoggingConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.log.audit", LOG_AUDIT, "true");
	AION_BIND(p, "gameserver.log.craft", LOG_CRAFT, "true");
	AION_BIND(p, "gameserver.log.gmaudit", LOG_GMAUDIT, "true");
	AION_BIND(p, "gameserver.log.chats.general", LOG_GENERAL_CHATS, "true");
	AION_BIND(p, "gameserver.log.chats.private", LOG_PRIVATE_CHATS, "false");
	AION_BIND(p, "gameserver.log.item", LOG_ITEM, "true");
	AION_BIND(p, "gameserver.log.kill", LOG_KILL, "false");
	AION_BIND(p, "gameserver.log.pl", LOG_PL, "false");
	AION_BIND(p, "gameserver.log.mail", LOG_MAIL, "false");
	AION_BIND(p, "gameserver.log.player.exchange", LOG_PLAYER_EXCHANGE, "false");
	AION_BIND(p, "gameserver.log.broker.exchange", LOG_BROKER_EXCHANGE, "false");
	AION_BIND(p, "gameserver.log.siege", LOG_SIEGE, "false");
	AION_BIND(p, "gameserver.log.sysmail", LOG_SYSMAIL, "false");
	AION_BIND(p, "gameserver.log.auction", LOG_HOUSE_AUCTION, "true");
	AION_BIND(p, "gameserver.log.tampering", LOG_TAMPERING, "false");
}

} // namespace aion::gameserver::configs::main
