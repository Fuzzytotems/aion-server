#pragma once

#include <optional>
#include <string_view>

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///givetitle: gives the target (or the GM) a title, if the GM may use //set.
 */
class Givetitle : public ConsoleCommand {
public:
	Givetitle();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& admin, std::optional<std::string_view> message) override;

private:
	/** @return whether the title was added (correction J-S4CP4-2; Java: void) */
	bool setTitle(Player& player, int32_t value);
};

} // namespace aion::gameserver::handlers::consolecommands
