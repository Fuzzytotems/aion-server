#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //damagepercent: damages your target's HP, MP, FP or DP by a percentage of the max (max HP, MP or FP; 4000 for DP, as Java's //damage).
 * <p>
 * C++-dev only, not in Java: Java's //damage took both points and percentages (Damage.java:79-80, any value up to 100 was a percentage);
 * C++-dev splits it into //damage (points) and this command (docs/deviations/C1.md). The access level is //damage's (getAliasForLevel), so
 * commands.properties needs no new key.
 */
class DamagePercent : public AdminCommand {
public:
	DamagePercent();

	void execute(Player& player, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;

protected:
	std::string getAliasForLevel() override;

private:
	/** "mp", "fp" or "dp" for those parameters (any case), else "hp" (Java //damage's damage type of the first parameter) */
	static std::string damageTypeOf(const std::string& param);
	/** Java //damage's damage paths with `percent` percent of the max (max HP, MP or FP; 4000 for DP) */
	static void apply(Creature& creature, runtime::Ptr<Player> player, const std::string& damageType, int32_t percent);
};

} // namespace aion::gameserver::handlers::admincommands
