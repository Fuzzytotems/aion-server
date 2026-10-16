#pragma once

#include <string_view>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //legion: modifies a legion.
 * <p>
 * Java: data/handlers/admincommands/LegionCommand.java. M5h-2 H2-5.
 */
class LegionCommand : public AdminCommand {
public:
	LegionCommand();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	/** Java: getLegion - '_' as ' ', the lower-case name; IllegalArgumentException("Legion <name> does not exist.") without one */
	static runtime::Ptr<model::team::legion::Legion> getLegion(std::string_view name);
};

} // namespace aion::gameserver::handlers::admincommands
