#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //house: house teleport and ownership management.
 * <p>
 * Java: data/handlers/admincommands/HouseCommand.java. M5h-2 H2-5.
 */
class HouseCommand : public AdminCommand {
public:
	HouseCommand();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	void listHouses(Player& admin);

	static std::string formatAddresses(const std::vector<runtime::Ptr<House>>& houses);

	/** Java: groupByType - LinkedHashMap in the order of the sort (type id descending, then address): a vector of (type, houses) */
	static std::vector<std::pair<HouseType, std::vector<runtime::Ptr<House>>>> groupByType(std::vector<runtime::Ptr<House>> houses);

	static runtime::Ptr<House> getHouse(std::string_view param);

	void acquireHouse(Player& admin, House& house);

	void revokeOwnership(Player& admin, House& house);

	void reloadPlayerScripts(Player& admin, House& house);
};

} // namespace aion::gameserver::handlers::admincommands
