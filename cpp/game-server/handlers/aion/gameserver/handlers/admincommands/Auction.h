#pragma once

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //auction: adds or removes houses to/from auction.
 * <p>
 * Java: data/handlers/admincommands/Auction.java. M5h-2 H2-5.
 */
class Auction : public AdminCommand {
public:
	Auction();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	/** Java: findHousesInZone, null for an invalid zone name (the admin is told) */
	std::optional<std::vector<runtime::Ptr<House>>> findHousesInZone(Player& admin, std::string_view zoneName, const std::function<bool(House&)>& filter);

	/** Java: String.matches("\\d+") */
	static bool isDigits(std::string_view s);
};

} // namespace aion::gameserver::handlers::admincommands
