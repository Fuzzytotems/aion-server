#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //damage: damages your target's HP, MP, FP or DP by a number of points.
 * <p>
 * C++-dev: Java's //damage also took a percentage (a trailing % or any value up to 100); that is //damagepercent now (DamagePercent.h,
 * docs/deviations/C1.md).
 */
class Damage : public AdminCommand {
public:
	Damage();

	void execute(Player& player, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;
};

} // namespace aion::gameserver::handlers::admincommands
