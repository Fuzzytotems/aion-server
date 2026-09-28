#include "aion/gameserver/network/aion/iteminfo/PlumeInfoBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/detail/ItemData.h"

namespace aion::gameserver::network::aion::iteminfo {

PlumeInfoBlobEntry::PlumeInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::PLUME_INFO) {
}

PlumeInfoBlobEntry::~PlumeInfoBlobEntry() = default;

runtime::Ref<PlumeInfoBlobEntry> PlumeInfoBlobEntry::create() {
	return runtime::makeRef<PlumeInfoBlobEntry>();
}

void PlumeInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	writeQ(buf, network::detail::slotIdMaskOf(network::detail::slotFor(ownerItem->getItemTemplate()->getItemSlot())));
	writeQ(buf, 0x100000); // secondary slot ?
	writeD(buf, 0); // unks
	writeD(buf, 0);
	writeD(buf, 0);
	writeD(buf, 0);
}

int32_t PlumeInfoBlobEntry::getSize() {
	return 32;
}

} // namespace aion::gameserver::network::aion::iteminfo
