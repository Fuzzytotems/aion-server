#include "aion/gameserver/network/aion/iteminfo/WeaponInfoBlobEntry.h"

#include <vector>

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/detail/ItemData.h"

namespace aion::gameserver::network::aion::iteminfo {

WeaponInfoBlobEntry::WeaponInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::SLOTS_WEAPON) {
}

WeaponInfoBlobEntry::~WeaponInfoBlobEntry() = default;

runtime::Ref<WeaponInfoBlobEntry> WeaponInfoBlobEntry::create() {
	return runtime::makeRef<WeaponInfoBlobEntry>();
}

void WeaponInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	runtime::Ptr<model::gameobjects::Item> item = ownerItem.get();
	std::vector<model::items::ItemSlot> slots = network::detail::slotsFor(item->getItemTemplate()->getItemSlot());
	if (slots.size() == 1) {
		writeQ(buf, network::detail::slotIdMaskOf(slots[0]));
		writeQ(buf, item->hasFusionedItem() ? 0x00 : 0x02);
		return;
	}
	if (item->getItemTemplate()->isTwoHandWeapon()) {
		// must occupy two slots
		writeQ(buf, network::detail::slotIdMaskOf(slots.at(0)) | network::detail::slotIdMaskOf(slots.at(1)));
		writeQ(buf, 0);
	} else {
		// primary and secondary slots
		writeQ(buf, network::detail::slotIdMaskOf(slots.at(0)));
		writeQ(buf, network::detail::slotIdMaskOf(slots.at(1)));
	}
}

int32_t WeaponInfoBlobEntry::getSize() {
	return 16;
}

} // namespace aion::gameserver::network::aion::iteminfo
