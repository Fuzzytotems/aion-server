#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //addcube: expands a player's cube.
 */
class AddCube : public AdminCommand {
public:
	AddCube();

	void execute(Player& player, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
