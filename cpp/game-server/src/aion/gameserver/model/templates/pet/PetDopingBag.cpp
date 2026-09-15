#include "aion/gameserver/model/templates/pet/PetDopingBag.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::model::templates::pet {

PetDopingBag::PetDopingBag() = default;

PetDopingBag::~PetDopingBag() = default;

runtime::Ref<PetDopingBag> PetDopingBag::create() {
	return runtime::makeRef<PetDopingBag>();
}

void PetDopingBag::setFoodItem(int32_t itemId) {
	setItem(itemId, 0);
}

int32_t PetDopingBag::getFoodItem() {
	runtime::Ptr<runtime::Array<int32_t>> bag = itemBag.get();
	if (bag == nullptr || bag->length() < 1)
		return 0;
	return bag->get(0);
}

void PetDopingBag::setDrinkItem(int32_t itemId) {
	setItem(itemId, 1);
}

int32_t PetDopingBag::getDrinkItem() {
	runtime::Ptr<runtime::Array<int32_t>> bag = itemBag.get();
	if (bag == nullptr || bag->length() < 2)
		return 0;
	return bag->get(1);
}

void PetDopingBag::setItem(int32_t itemId, int32_t slot) {
	SYNCHRONIZED(*this) {
		if (slot < 0 || slot >= MAX_ITEMS)
			throw runtime::IllegalArgumentException("Slot index " + std::to_string(slot) + " for item " + std::to_string(itemId) + " is invalid.");
		runtime::Ptr<runtime::Array<int32_t>> bag = itemBag.get();
		if (bag == nullptr || slot >= bag->length()) {
			// Java: itemBag == null ? new int[slot + 1] : Arrays.copyOf(itemBag, slot + 1)
			runtime::Ref<runtime::Array<int32_t>> grown = runtime::Array<int32_t>::make(slot + 1);
			if (bag != nullptr) {
				int32_t copied = std::min(bag->length(), slot + 1);
				for (int32_t i = 0; i < copied; ++i)
					(*grown)[i] = bag->get(i);
			}
			bag = grown;
			itemBag.set(std::move(grown));
		}
		if (bag->get(slot) != itemId) {
			(*bag)[slot] = itemId;
			isDirty_.set(true);
		}
	}
}

std::vector<int32_t> PetDopingBag::getScrollsUsed() {
	runtime::Ptr<runtime::Array<int32_t>> bag = itemBag.get();
	if (bag == nullptr || bag->length() < 3)
		return {};
	std::vector<int32_t> scrolls; // Java: Arrays.copyOfRange(itemBag, 2, itemBag.length)
	for (int32_t i = 2; i < bag->length(); ++i)
		scrolls.push_back(bag->get(i));
	return scrolls;
}

std::vector<int32_t> PetDopingBag::getItems() {
	runtime::Ptr<runtime::Array<int32_t>> bag = itemBag.get();
	if (bag == nullptr)
		return {};
	std::vector<int32_t> items;
	for (int32_t i = 0; i < bag->length(); ++i)
		items.push_back(bag->get(i));
	return items;
}

void PetDopingBag::switchItems(int32_t slot1, int32_t slot2) {
	if (slot1 < 2 || slot2 < 2)
		return;
	// java-race: reads both slots without the lock, then writes them with two separate synchronized setItem calls
	runtime::Ptr<runtime::Array<int32_t>> bag = itemBag.get();
	if (bag == nullptr)
		throw runtime::NullPointerException("PetDopingBag.itemBag is null"); // Java: itemBag.length on a null array
	int32_t slot1Item = bag->length() > slot1 ? bag->get(slot1) : 0;
	int32_t slot2Item = bag->length() > slot2 ? bag->get(slot2) : 0;
	setItem(slot1Item, slot2);
	setItem(slot2Item, slot1);
}

} // namespace aion::gameserver::model::templates::pet
