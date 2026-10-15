#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///set_enchantcount: raises the enchantment level of one of the GM's items.
 *
 * @author ginho1
 */
class Set_enchantcount : public ConsoleCommand {
public:
	Set_enchantcount();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& admin, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::consolecommands
