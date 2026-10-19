#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //sys: shows and controls the system environment (information, memory, the thread pool, restart and shutdown).
 *
 * @author lord_rex
 */
class Sys : public AdminCommand {
public:
	Sys();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	static void initShutdown(int32_t exitCode, std::optional<std::string> delay);
};

} // namespace aion::gameserver::handlers::admincommands
