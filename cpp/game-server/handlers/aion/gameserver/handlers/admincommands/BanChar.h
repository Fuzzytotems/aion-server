#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //banchar: bans a character for a number of days. @author nrg
 */
class BanChar : public AdminCommand {
public:
	BanChar();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
private:
	void sendInfo(Player& player, bool withNote);
};

} // namespace aion::gameserver::handlers::admincommands
