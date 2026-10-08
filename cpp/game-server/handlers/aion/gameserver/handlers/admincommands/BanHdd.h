#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //banhdd: bans a hard disk serial. @author ViAl
 */
class BanHdd : public AdminCommand {
public:
	BanHdd();

	void execute(Player& admin, std::span<const std::string> params) override;
private:
	static constexpr std::string_view SYNTAX = "Syntax: //banhdd <hdd_serial> <time_in_minutes|0 - infinite>";
};

} // namespace aion::gameserver::handlers::admincommands
