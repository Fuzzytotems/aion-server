#include "aion/gameserver/dataholders/HousePartsData.h"

namespace aion::gameserver::dataholders {

void HousePartsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// Java: if (houseParts == null) return; (an empty list builds no index either)
	for (const model::templates::housing::HousePart& part : houseParts)
		partsById.insert_or_assign(part.getId(), &part);
	// Java: houseParts = null (the C++ index points into the storage, which stays)
}

const model::templates::housing::HousePart* HousePartsData::getPartById(int32_t partId) const {
	auto it = partsById.find(partId);
	return it != partsById.end() ? it->second : nullptr;
}

int32_t HousePartsData::size() const {
	return static_cast<int32_t>(partsById.size());
}

} // namespace aion::gameserver::dataholders
