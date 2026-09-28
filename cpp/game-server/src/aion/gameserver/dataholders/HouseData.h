#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/HouseData.xml.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.HouseData.
 * <p>
 * C++: the index points into the bound lands, which Java keeps too. A duplicate address fails the load through LoadContext::fail with Java's
 * IllegalArgumentException message. getAddresses walks the addresses in Java's HashMap<Integer, HouseAddress> iteration order.
 *
 * @author Rolandas
 */
class HouseData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HouseData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::housing::HouseAddress*> addressesById;
	/** C++ only: addressesById.values() in Java's HashMap iteration order */
	std::vector<const model::templates::housing::HouseAddress*> addressesInHashOrder;

public:
	std::vector<const model::templates::housing::HouseAddress*> getAddresses(int32_t worldId) const;

	/** @return the address, nullptr (Java null) if there is none */
	const model::templates::housing::HouseAddress* getAddress(int32_t houseAddress) const;

	const model::templates::housing::HouseAddress* getStudioAddress(model::Race race) const;

	const std::vector<model::templates::housing::HousingLand>& getLands() const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
