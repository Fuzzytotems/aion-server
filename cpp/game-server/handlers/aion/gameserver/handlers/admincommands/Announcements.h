#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //announcements: manages the automatic announcements.
 *
 * @author Divinity
 */
class Announcements : public AdminCommand {
public:
	Announcements();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	static std::string replace(std::string s, std::string_view from, std::string_view to);
};

} // namespace aion::gameserver::handlers::admincommands
