#include "aion/gameserver/model/templates/pet/PetDopingBag.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::pet {

PetDopingBag::PetDopingBag() = default;

PetDopingBag::~PetDopingBag() = default;

runtime::Ref<PetDopingBag> PetDopingBag::create() {
	return runtime::makeRef<PetDopingBag>();
}

void PetDopingBag::setFoodItem(int32_t itemId) {
	AION_UNPORTED();
}

int32_t PetDopingBag::getFoodItem() {
	AION_UNPORTED();
}

void PetDopingBag::setDrinkItem(int32_t itemId) {
	AION_UNPORTED();
}

int32_t PetDopingBag::getDrinkItem() {
	AION_UNPORTED();
}

void PetDopingBag::setItem(int32_t itemId, int32_t slot) {
	AION_UNPORTED();
}

std::vector<int32_t> PetDopingBag::getScrollsUsed() {
	AION_UNPORTED();
}

std::vector<int32_t> PetDopingBag::getItems() {
	AION_UNPORTED();
}

void PetDopingBag::switchItems(int32_t slot1, int32_t slot2) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::pet
