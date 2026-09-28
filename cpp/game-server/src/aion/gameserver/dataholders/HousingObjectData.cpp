#include "aion/gameserver/dataholders/HousingObjectData.h"

namespace aion::gameserver::dataholders {

void HousingObjectData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const std::unique_ptr<model::templates::housing::PlaceableHouseObject>& obj : housingObjects)
		objectTemplatesById.insert_or_assign(obj->getTemplateId(), obj.get());
	// Java: housingObjects = null (the C++ index points into the storage, which stays)
}

int32_t HousingObjectData::size() const {
	return static_cast<int32_t>(objectTemplatesById.size());
}

const model::templates::housing::PlaceableHouseObject* HousingObjectData::getTemplateById(int32_t templateId) const {
	auto it = objectTemplatesById.find(templateId);
	return it != objectTemplatesById.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
