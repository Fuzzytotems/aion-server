#pragma once

#include <optional>
#include <string_view>

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///wishid: adds an item by id to the target (or the GM).
 */
class Wishid : public ConsoleCommand {
public:
	Wishid();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& admin, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::consolecommands
