#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///addquest: starts a quest for the GM's target (or the GM).
 *
 * @author ginho1
 */
class Addquest : public ConsoleCommand {
public:
	Addquest();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::consolecommands
