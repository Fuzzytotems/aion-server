#include "aion/chatserver/configs/main/LoggingConfig.h"

#include "aion/commons/configuration/ConfigurableProcessor.h"

namespace aion::chatserver::configs::main {

void LoggingConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	p.bind("chatserver.log.channel.request", LOG_CHANNEL_REQUEST, "false");
	p.bind("chatserver.log.channel.invalid", LOG_CHANNEL_INVALID, "false");
	p.bind("chatserver.log.chat", LOG_CHAT, "false");
	p.bind("chatserver.log.chat_to_db", LOG_CHAT_TO_DB, "false");
}

} // namespace aion::chatserver::configs::main
