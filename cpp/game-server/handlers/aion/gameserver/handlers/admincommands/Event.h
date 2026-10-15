#pragma once

#include <optional>
#include <string>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //event: manages event functions and the players' event states.
 */
class Event : public AdminCommand {
public:
	Event();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	runtime::Ptr<Player> getPlayer(Player& admin, std::optional<std::string> name);

	void clearInstance(Player& admin);

	void setEventState(Player& admin, Player& player, bool onlyRemove);
};

} // namespace aion::gameserver::handlers::admincommands
