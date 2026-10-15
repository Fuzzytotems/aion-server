#pragma once

#include <vector>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //playerinfo: shows information about a player.
 *
 * @author lyahim, antness
 */
class PlayerInfo : public AdminCommand {
public:
	PlayerInfo();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	void appendItems(std::string& strbld, const std::vector<runtime::Ptr<Item>>& items);
};

} // namespace aion::gameserver::handlers::admincommands
