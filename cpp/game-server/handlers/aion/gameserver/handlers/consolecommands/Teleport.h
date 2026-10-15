#pragma once

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///teleport: sent by the GM dialog's bookmarks and the console's world map click; moves the GM to a position.
 *
 * @author Yeats
 */
class Teleport : public ConsoleCommand {
public:
	Teleport();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	runtime::Ptr<WorldMapInstance> getOrCreateWorldMapInstance(Player& player, int32_t worldId);
};

} // namespace aion::gameserver::handlers::consolecommands
