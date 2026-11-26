#include "aion/gameserver/dataholders/PetData.h"

#include <string>

#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/AdoptPetAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"

namespace aion::gameserver::dataholders {

// Java PetData.java:29-35
void PetData::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, const model::templates::pet::PetTemplate*> order;
	for (const model::templates::pet::PetTemplate& pet : pets) {
		petData.insert_or_assign(pet.getTemplateId(), &pet);
		order.put(pet.getTemplateId(), &pet, detail::javaHashCode(pet.getTemplateId()));
	}
	petIdsInHashOrder = order.keys();
	// Java: pets = null (the C++ index points into the storage, which stays)
	// Java: validatePetIds(StaticDataListener.getItemData(u)) (upstream 4426db07c). C++: the item templates of this load or the published
	// ITEM_DATA; a holder bound without any item data (unit tests) skips the check, where Java throws NullPointerException
	// (docs/deviations/P4-09.md)
	if (const ItemData* itemData = ctx.findHolder<ItemData>())
		validatePetIds(ctx, *itemData);
}

// Java PetData.java:37-47
void PetData::validatePetIds(xml::LoadContext& ctx, const ItemData& itemData) const {
	for (const model::templates::item::ItemTemplate* item : itemData.getItemTemplates()) {
		if (item->getActions() == nullptr)
			continue;
		const model::templates::item::actions::AdoptPetAction* adoptPetAction = item->getActions()->getAdoptPetAction();
		if (adoptPetAction == nullptr)
			continue;
		if (getPetTemplate(adoptPetAction->getPetId()) == nullptr)
			ctx.fail("Missing pet template " + std::to_string(adoptPetAction->getPetId()) + " for item " + std::to_string(item->getTemplateId()));
	}
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
