#include "aion/gameserver/handlers/admincommands/HouseCommand.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/templates/housing/HouseTypeInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(HouseCommand);

HouseCommand::HouseCommand()
	: AdminCommand("house", "House teleport and ownership management.", R"(list - Shows house addresses for each map.
tp <address> - Teleports you to the house with the given address.
own <address> - Gives ownership of given house to your target.
revoke <address> - Revokes ownership of given house.
reloadscripts <address> - Reloads all scripts for the given house.
)") {
}

// Java HouseCommand.java:39-55
void HouseCommand::execute(Player& admin, std::span<const std::string> params) {
	namespace StringUtils = commons::utils::StringUtils;
	if (params.size() >= 1 && StringUtils::equalsIgnoreCase("list", params[0])) {
		listHouses(admin);
	} else if (params.size() >= 2 && StringUtils::equalsIgnoreCase("own", params[0])) {
		acquireHouse(admin, *getHouse(params[1]));
	} else if (params.size() >= 2 && StringUtils::equalsIgnoreCase("revoke", params[0])) {
		revokeOwnership(admin, *getHouse(params[1]));
	} else if (params.size() >= 2 && StringUtils::equalsIgnoreCase("tp", params[0])) {
		runtime::Ptr<House> house = getHouse(params[1]);
		TeleportService::teleportTo(admin, *house->getPosition()->getWorldMapInstance(), house->getX(), house->getY(), house->getZ(),
			house->getTeleportHeading());
	} else if (params.size() >= 2 && StringUtils::equalsIgnoreCase("reloadscripts", params[0])) {
		reloadPlayerScripts(admin, *getHouse(params[1]));
	} else {
		sendInfo(admin);
	}
}

// Java HouseCommand.java:57-67
void HouseCommand::listHouses(Player& admin) {
	std::map<int32_t, std::vector<runtime::Ptr<House>>> byMap; // Java: groupingBy(mapId, TreeMap::new, toList())
	for (const runtime::Ptr<House>& house : HousingService::getInstance().getCustomHouses())
		byMap[house->getAddress()->getMapId()].push_back(house);
	for (const auto& [mapId, houses] : byMap) {
		sendInfo(admin, "House addresses in " + worldName(mapId) + ":");
		for (const auto& [houseType, housesOfType] : groupByType(houses)) {
			const std::string name(xml::enumName(houseType));
			const std::string houseTypeName = name.substr(0, 1) + commons::utils::StringUtils::toLowerCase(name.substr(1));
			sendInfo(admin, "\t" + houseTypeName + ": " + formatAddresses(housesOfType));
		}
	}
}

// Java HouseCommand.java:69-89
std::string HouseCommand::formatAddresses(const std::vector<runtime::Ptr<House>>& houses) {
	bool dash = false;
	int32_t lastAddress = houses.front()->getAddress()->getId(); // Java: houses.getFirst()
	std::string addresses = ChatUtil::color(std::to_string(lastAddress), utils::JavaColor::WHITE);
	for (size_t i = 1; i < houses.size(); i++) {
		const runtime::Ptr<House>& house = houses[i];
		const int32_t currentAddress = house->getAddress()->getId();
		if (lastAddress + 1 != currentAddress || i + 1 == houses.size() || currentAddress + 1 != houses[i + 1]->getAddress()->getId()) {
			if (!addresses.empty() && !dash)
				addresses += ", ";
			addresses += ChatUtil::color(std::to_string(currentAddress), utils::JavaColor::WHITE);
			dash = false;
		} else if (!dash) {
			addresses += "-";
			dash = true;
		}
		lastAddress = house->getAddress()->getId();
	}
	return addresses;
}

// Java HouseCommand.java:91-95
std::vector<std::pair<HouseType, std::vector<runtime::Ptr<House>>>> HouseCommand::groupByType(std::vector<runtime::Ptr<House>> houses) {
	// Java: sorted(comparing(type id).reversed().thenComparing(address id)) - a stable sort, so equal keys keep their order
	std::stable_sort(houses.begin(), houses.end(), [](const runtime::Ptr<House>& a, const runtime::Ptr<House>& b) {
		const int32_t typeA = model::templates::housing::getId(a->getHouseType());
		const int32_t typeB = model::templates::housing::getId(b->getHouseType());
		if (typeA != typeB)
			return typeA > typeB;
		return a->getAddress()->getId() < b->getAddress()->getId();
	});
	std::vector<std::pair<HouseType, std::vector<runtime::Ptr<House>>>> grouped; // Java: groupingBy(House::getHouseType, LinkedHashMap::new, toList())
	for (const runtime::Ptr<House>& house : houses) {
		const HouseType type = house->getHouseType();
		auto group = std::find_if(grouped.begin(), grouped.end(), [type](const auto& entry) { return entry.first == type; });
		if (group == grouped.end()) {
			grouped.emplace_back(type, std::vector<runtime::Ptr<House>>{});
			group = std::prev(grouped.end());
		}
		group->second.push_back(house);
	}
	return grouped;
}

// Java HouseCommand.java:97-100
runtime::Ptr<House> HouseCommand::getHouse(std::string_view param) {
	const int32_t address = commons::utils::parseInt(param);
	runtime::Ptr<House> house = HousingService::getInstance().getHouseByAddress(address);
	if (!house)
		throw runtime::NullPointerException("Invalid address."); // Java: Objects.requireNonNull(..., "Invalid address.")
	return house;
}

// Java HouseCommand.java:102-122
void HouseCommand::acquireHouse(Player& admin, House& house) {
	const runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget());
	if (!target) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return;
	}

	if (house.getOwnerId() == target->getObjectId()) {
		sendInfo(admin, name(*target) + " already owns that house.");
		return;
	}
	if (target->getHouses()->size() >= 2) {
		sendInfo(admin, name(*target) + " must sell his old house which is currently in grace time first!");
		return;
	}
	runtime::Ptr<House> studio = HousingService::getInstance().getPlayerStudio(target->getObjectId());
	if (studio)
		HousingService::getInstance().changeOwner(*studio, 0);
	HousingService::getInstance().changeOwner(house, target->getObjectId());
	sendInfo(admin, "House " + house.getName() + " is now owned by " + name(*target));
}

// Java HouseCommand.java:124-132
void HouseCommand::revokeOwnership(Player& admin, House& house) {
	const int32_t ownerId = house.getOwnerId();
	if (ownerId == 0) {
		sendInfo(admin, "House has no owner.");
		return;
	}
	HousingService::getInstance().changeOwner(house, 0);
	// Java string concatenation of a null name: "null"
	sendInfo(admin, "Ownership of house " + std::to_string(house.getAddress()->getId()) + " was revoked from " +
						PlayerService::getPlayerName(ownerId).value_or("null"));
}

// Java HouseCommand.java:134-144
void HouseCommand::reloadPlayerScripts(Player& admin, House& house) {
	runtime::Ptr<Npc> butler = house.getButler();
	if (!butler) {
		sendInfo(admin, "No butler was found for house with address " + std::to_string(house.getAddress()->getId()));
		return;
	}
	house.reloadPlayerScripts();
	butler->getKnownList().forEachPlayer([&house](Player& player) { house.sendScripts(player); });
	sendInfo(admin, "Script reload successful");
}

} // namespace aion::gameserver::handlers::admincommands
