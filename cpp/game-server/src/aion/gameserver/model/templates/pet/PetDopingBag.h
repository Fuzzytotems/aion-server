#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/templates/pet/fwd.h"

namespace aion::gameserver::model::templates::pet {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). Not a template despite its package: RefCounted player data (fieldmap K4,
 * `PetCommonData.dopingBag`), created with create(). getItems() and getScrollsUsed() return copies of the item ids (`std::vector`, §6).
 *
 * @author Rolandas
 */
class PetDopingBag : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	static constexpr int32_t MAX_ITEMS = 8; // food slot, drink slot and 6 scroll slots

private:
	runtime::Field<runtime::Ref<runtime::Array<int32_t>>> itemBag{};
	runtime::Field<bool> isDirty_{false};

protected:
	PetDopingBag();
	~PetDopingBag() override;

public:
	/** Java: new PetDopingBag() */
	static runtime::Ref<PetDopingBag> create();

	void setFoodItem(int32_t itemId);

	int32_t getFoodItem();

	void setDrinkItem(int32_t itemId);

	int32_t getDrinkItem();

	/**
	 * Adds or removes item to the bag (synchronized)
	 *
	 * @param itemId - item Id, or 0 to remove
	 * @param slot - slot number; 0 for food, 1 for drink, the rest are for scrolls
	 */
	void setItem(int32_t itemId, int32_t slot);

	std::vector<int32_t> getScrollsUsed();

	std::vector<int32_t> getItems();

	/** Currently only scrolls can be relocated */
	void switchItems(int32_t slot1, int32_t slot2);

	/** @return true if the bag needs saving */
	bool isDirty() const { return isDirty_.get(); }
};

} // namespace aion::gameserver::model::templates::pet
