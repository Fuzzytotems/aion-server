#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///changeclass: changes a player's class.
 *
 * @author ginho1, Neon
 */
class Changeclass : public ConsoleCommand {
public:
	Changeclass();

	void execute(Player& admin, std::span<const std::string> params) override;

	/** Java: protected static, read by Classup */
	static PlayerClass parsePlayerClass(const std::string& param);
};

} // namespace aion::gameserver::handlers::consolecommands
