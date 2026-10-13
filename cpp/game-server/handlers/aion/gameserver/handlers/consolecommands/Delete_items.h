#pragma once

#include <optional>
#include <string_view>

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///delete_items: deletes every inventory item of the target (or the GM) up to the given quality.
 */
class Delete_items : public ConsoleCommand {
public:
	Delete_items();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& admin, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::consolecommands
