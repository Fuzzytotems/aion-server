#include "aion/gameserver/network/aion/iteminfo/WingInfoBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/detail/ItemData.h"

namespace aion::gameserver::network::aion::iteminfo {

WingInfoBlobEntry::WingInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::SLOTS_WING) {
}

WingInfoBlobEntry::~WingInfoBlobEntry() = default;

runtime::Ref<WingInfoBlobEntry> WingInfoBlobEntry::create() {
	return runtime::makeRef<WingInfoBlobEntry>();
}

void WingInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	writeQ(buf, network::detail::slotIdMaskOf(network::detail::slotFor(ownerItem->getItemTemplate()->getItemSlot())));
	writeQ(buf, 0); // no secondary slot
}

int32_t WingInfoBlobEntry::getSize() {
	return 16;
}

} // namespace aion::gameserver::network::aion::iteminfo
