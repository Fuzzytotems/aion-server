#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //bookmark: deletes teleport bookmarks (the GM console's Bookmark_add adds them).
 */
class Bookmark : public AdminCommand {
public:
	Bookmark();

	void execute(Player& player, std::span<const std::string> params) override;

protected:
	std::string getAliasForLevel() override;
};

} // namespace aion::gameserver::handlers::admincommands
