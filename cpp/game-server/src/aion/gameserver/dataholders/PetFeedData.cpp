#include "aion/gameserver/dataholders/PetFeedData.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"

namespace aion::gameserver::dataholders {

using model::templates::pet::PetFlavour;

void PetFeedData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// Java: if (flavours == null) return; (an empty list builds no index either)
	detail::JavaHashMapOrder<int32_t, const PetFlavour*> order;
	for (const PetFlavour& flavour : flavours) {
		petFlavoursById.insert_or_assign(flavour.getId(), &flavour);
		order.put(flavour.getId(), &flavour, detail::javaHashCode(flavour.getId()));
	}
	flavoursInHashOrder = order.values();
	// Java: flavours.clear(); flavours = null (the C++ index points into the storage, which stays)
}

const PetFlavour* PetFeedData::getFlavourById(int32_t flavourId) const {
	auto it = petFlavoursById.find(flavourId);
	return it != petFlavoursById.end() ? it->second : nullptr;
}

int32_t PetFeedData::size() const {
	return static_cast<int32_t>(petFlavoursById.size());
}

std::vector<const PetFlavour*> PetFeedData::getPetFlavours() const {
	return flavoursInHashOrder;
}

} // namespace aion::gameserver::dataholders
