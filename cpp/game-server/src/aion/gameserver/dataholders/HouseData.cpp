#include "aion/gameserver/dataholders/HouseData.h"

#include <string>

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/Race.h"

namespace aion::gameserver::dataholders {

using model::templates::housing::HouseAddress;
using model::templates::housing::HousingLand;

void HouseData::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, const HouseAddress*> order;
	for (const HousingLand& land : lands) {
		if (!land.getAddresses())
			continue; // required wrapper: never absent after strict binding
		for (const HouseAddress& address : *land.getAddresses()) {
			if (!addressesById.insert_or_assign(address.getId(), &address).second) // Java: addressesById.put(...) != null
				ctx.fail("Duplicate house address " + std::to_string(address.getId()) + " in house_lands templates");
			order.put(address.getId(), &address, detail::javaHashCode(address.getId()));
		}
	}
	addressesInHashOrder = order.values();
}

std::vector<const HouseAddress*> HouseData::getAddresses(int32_t worldId) const {
	std::vector<const HouseAddress*> addresses;
	for (const HouseAddress* address : addressesInHashOrder) {
		if (address->getMapId() == worldId)
			addresses.push_back(address);
	}
	return addresses;
}

const HouseAddress* HouseData::getAddress(int32_t houseAddress) const {
	auto it = addressesById.find(houseAddress);
	return it != addressesById.end() ? it->second : nullptr;
}

const HouseAddress* HouseData::getStudioAddress(model::Race race) const {
	return getAddress(race == model::Race::ELYOS ? 2001 : 3001);
}

const std::vector<HousingLand>& HouseData::getLands() const {
	return lands;
}

int32_t HouseData::size() const {
	return static_cast<int32_t>(lands.size());
}

} // namespace aion::gameserver::dataholders
