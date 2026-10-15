#include "aion/gameserver/dataholders/FlyPathData.h"

namespace aion::gameserver::dataholders {

void FlyPathData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::flypath::FlyPathEntry& loc : list)
		loctlistData.insert_or_assign(loc.getId(), &loc);
	// Java: list = null (the C++ index points into the storage, which stays)
}

int32_t FlyPathData::size() const {
	return static_cast<int32_t>(loctlistData.size());
}

const model::templates::flypath::FlyPathEntry* FlyPathData::getPathTemplate(int32_t id) const {
	auto it = loctlistData.find(id);
	return it != loctlistData.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
