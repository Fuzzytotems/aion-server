#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //grant: grants or revokes account permissions. @author Neon
 */
class Grant : public AdminCommand {
public:
	Grant();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
