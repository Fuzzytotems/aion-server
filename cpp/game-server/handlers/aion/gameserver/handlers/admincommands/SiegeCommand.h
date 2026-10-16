#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //siege: controls sieges and artifacts.
 * <p>
 * Java: data/handlers/admincommands/SiegeCommand.java. M5i I-1.
 */
class SiegeCommand : public AdminCommand {
public:
	SiegeCommand();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	void listLocations(Player& player);

	void startSiege(Player& player, SiegeLocation& loc);

	void stopSiege(Player& player, SiegeLocation& loc);

	void capture(Player& player, SiegeLocation& loc, std::span<const std::string> params);

	void assault(Player& player, SiegeLocation& loc, int32_t delaySeconds);

	/** @throws IllegalArgumentException "Invalid location ID." without a location id or for an unknown one */
	runtime::Ptr<SiegeLocation> parseLocation(std::span<const std::string> params);

	/** Java: returns Object (the l10n string, or "<type> <id>" without one) */
	static std::string getLocationName(SiegeLocation& loc);
};

} // namespace aion::gameserver::handlers::admincommands
