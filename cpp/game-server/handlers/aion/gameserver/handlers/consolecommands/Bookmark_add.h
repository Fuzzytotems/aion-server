#pragma once

#include <string_view>

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///Bookmark_add: the GM console's "Bookmark" button, stores a teleport bookmark at the GM's position.
 *
 * @author ginho1
 */
class Bookmark_add : public ConsoleCommand {
public:
	static constexpr std::string_view ALIAS = "Bookmark_add";

	Bookmark_add();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::consolecommands
