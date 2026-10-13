#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///partyrecall: not implemented in Java either.
 */
class Partyrecall : public ConsoleCommand {
public:
	Partyrecall();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
