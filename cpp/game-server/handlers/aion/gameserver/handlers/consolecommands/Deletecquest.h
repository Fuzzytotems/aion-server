#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///deletecquest: deletes a quest from the target's (or the GM's) quest list through //quest.
 */
class Deletecquest : public ConsoleCommand {
public:
	Deletecquest();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
