#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //sprison: sends a player to prison. @author lord_rex
 */
class SPrison : public AdminCommand {
public:
	SPrison();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
private:
	void sendInfo(Player& player);
};

} // namespace aion::gameserver::handlers::admincommands
