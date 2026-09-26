#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/dataholders/PlayerInitialData.xml.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.PlayerInitialData.
 * <p>
 * C++: the index points into the bound `dataList` storage, which stays after afterUnmarshal (static-data.md §2.6). Java's HashMap key may be
 * null (a player_data without class), so the C++ key is optional.
 *
 * @author Aquanox
 */
class PlayerInitialData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PlayerInitialData.xml.inc"
private:
	std::map<std::optional<model::PlayerClass>, const PlayerCreationData*> data;

public:
	/** @return the creation data of the class, nullptr (Java null) if there is none */
	const PlayerCreationData* getPlayerCreationData(model::PlayerClass cls) const;

	int32_t size() const;

	/** @throws IllegalArgumentException for a race other than ELYOS and ASMODIANS */
	const LocationData& getSpawnLocation(model::Race race) const;
};

/** Java com.aionemu.gameserver.dataholders.PlayerInitialData.LocationData. Location data holder. */
class PlayerInitialData::LocationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PlayerInitialData_LocationData.xml.inc"
public:
};

/** Java com.aionemu.gameserver.dataholders.PlayerInitialData.PlayerCreationData. Player creation data holder. */
class PlayerInitialData::PlayerCreationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PlayerInitialData_PlayerCreationData.xml.inc"
public:
	/** Java: Collections.unmodifiableList(itemsType.items). @throws NullPointerException if the data has no items element */
	const std::vector<ItemType>& getItems() const;
};

/** Java com.aionemu.gameserver.dataholders.PlayerInitialData.PlayerCreationData.ItemType. */
class PlayerInitialData::PlayerCreationData::ItemType : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PlayerInitialData_PlayerCreationData_ItemType.xml.inc"
public:
	/** Java: "ItemType{template=" + template + ", count=" + count + '}' (the template as Java's Object.toString: class name @ identity) */
	std::string toString() const;
};

} // namespace aion::gameserver::dataholders

// the data-only ItemsType completes PlayerCreationData (its header includes this one)
#include "aion/gameserver/dataholders/PlayerInitialData_PlayerCreationData_ItemsType.h"
