#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //info: shows information about your target.
 */
class Info : public AdminCommand {
public:
	Info();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	std::string createZoneInfo(Creature& creature);
	std::string createAggroInfo(Creature& creature);
	/** Java: String.valueOf(float) */ static std::string floatStr(float value);
	/** Java: String.valueOf(boolean) */ static std::string boolStr(bool value);
	/** Java: stat.getCurrent() in a string concatenation */ static std::string current(const std::unique_ptr<model::stats::calc::Stat2>& stat);
};

} // namespace aion::gameserver::handlers::admincommands
