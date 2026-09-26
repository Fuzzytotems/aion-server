#include "aion/gameserver/dataholders/MultiReturnItemData.h"

namespace aion::gameserver::dataholders {

void MultiReturnItemData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	returnLocList.clear();
	for (const model::templates::item::MultiReturnItem& template_ : multiReturnItemTemplate)
		returnLocList.insert_or_assign(template_.getId(), &template_.getReturnLocList());
	// Java: multiReturnItemTemplate = null (the C++ index points into the storage, which stays)
}

int32_t MultiReturnItemData::size() const {
	return static_cast<int32_t>(returnLocList.size());
}

const std::vector<model::templates::item::ReturnLocList>* MultiReturnItemData::getReturnLocListById(int32_t id) const {
	auto it = returnLocList.find(id);
	return it != returnLocList.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
