#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///setinventorygrowth: expands the cube of the GM's target (or the GM).
 */
class Setinventorygrowth : public ConsoleCommand {
public:
	Setinventorygrowth();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& admin, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::consolecommands
