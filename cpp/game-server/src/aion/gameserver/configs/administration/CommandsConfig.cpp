#include "aion/gameserver/configs/administration/CommandsConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::administration {

void CommandsConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND_PATTERN(p, "^[a-zA-Z0-9_]+$", ACCESS_LEVELS);
	AION_BIND(p, "gameserver.commands.handler_directories", HANDLER_DIRECTORIES,
	          "./data/handlers/admincommands, ./data/handlers/playercommands, ./data/handlers/consolecommands");
}

} // namespace aion::gameserver::configs::administration
