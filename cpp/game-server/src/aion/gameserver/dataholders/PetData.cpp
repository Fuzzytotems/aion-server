#include "aion/gameserver/dataholders/PetData.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"

namespace aion::gameserver::dataholders {

void PetData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, const model::templates::pet::PetTemplate*> order;
	for (const model::templates::pet::PetTemplate& pet : pets) {
		petData.insert_or_assign(pet.getTemplateId(), &pet);
		order.put(pet.getTemplateId(), &pet, detail::javaHashCode(pet.getTemplateId()));
	}
	petIdsInHashOrder = order.keys();
	// Java: pets = null (the C++ index points into the storage, which stays)
}

int32_t PetData::size() const {
	return static_cast<int32_t>(petData.size());
}

const model::templates::pet::PetTemplate* PetData::getPetTemplate(int32_t id) const {
	auto it = petData.find(id);
	return it != petData.end() ? it->second : nullptr;
}

const std::vector<int32_t>& PetData::getPetIds() const {
	return petIdsInHashOrder;
}

} // namespace aion::gameserver::dataholders
