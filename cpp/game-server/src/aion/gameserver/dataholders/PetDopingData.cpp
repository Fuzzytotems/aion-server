#include "aion/gameserver/dataholders/PetDopingData.h"

namespace aion::gameserver::dataholders {

void PetDopingData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::pet::PetDopingEntry& dope : list)
		dopingsById.insert_or_assign(dope.getId(), &dope);
	// Java: list = null (the C++ index points into the storage, which stays)
}

int32_t PetDopingData::size() const {
	return static_cast<int32_t>(dopingsById.size());
}

const model::templates::pet::PetDopingEntry* PetDopingData::getDopingTemplate(int32_t id) const {
	auto it = dopingsById.find(id);
	return it != dopingsById.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
