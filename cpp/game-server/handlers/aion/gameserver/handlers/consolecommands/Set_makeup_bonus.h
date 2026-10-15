#pragma once

#include <optional>
#include <string_view>

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///set_makeup_bonus: sets the repose energy of the target (or the GM).
 */
class Set_makeup_bonus : public ConsoleCommand {
public:
	Set_makeup_bonus();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& admin, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::consolecommands
