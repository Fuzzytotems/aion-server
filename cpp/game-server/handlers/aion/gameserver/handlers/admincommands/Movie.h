#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //movie: plays movies/cutscenes.
 *
 * @author d3v1an
 */
class Movie : public AdminCommand {
public:
	Movie();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
