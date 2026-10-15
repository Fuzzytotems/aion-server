#pragma once

#include <optional>
#include <string_view>

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///endquest: completes a quest of the target (or the GM) without rewards.
 */
class Endquest : public ConsoleCommand {
public:
	Endquest();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
